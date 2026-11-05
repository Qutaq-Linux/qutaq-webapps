#include "mainwindow.h"

#include "i18n.h"
#include "webappdialog.h"
#include "webappmodel.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileSystemWatcher>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QAbstractButton>
#include <QMessageBox>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QToolButton *makeButton(const QStringList &iconNames, const QString &fallback, const QString &tip)
{
    auto *b = new QToolButton;
    QIcon icon;
    for (const QString &n : iconNames) {
        icon = QIcon::fromTheme(n);
        if (!icon.isNull())
            break;
    }
    if (icon.isNull()) {
        b->setText(fallback);
        QFont f = b->font();
        f.setPointSizeF(f.pointSizeF() * 1.3);
        b->setFont(f);
    } else {
        b->setIcon(icon);
    }
    b->setToolTip(tip);
    b->setAutoRaise(true);          // плоская, рамка только при наведении — как в PCManFM-Qt
    b->setFocusPolicy(Qt::TabFocus);
    return b;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_model(new WebAppModel(this)),
      m_proxy(new WebAppFilterModel(this)),
      m_watcher(new QFileSystemWatcher(this)),
      m_reloadTimer(new QTimer(this))
{
    setWindowTitle(tr("Web Apps"));
    resize(680, 520);

    // ---- верхняя панель: поиск + меню ---------------------------------------
    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search web apps…  (Ctrl+F)"));
    m_search->setClearButtonEnabled(true);
    // Внутри поля — монохромная лупа (как в PCManFM-Qt/GTK), цветная — запасной вариант.
    QIcon findIcon = QIcon::fromTheme(QStringLiteral("edit-find-symbolic"));
    if (findIcon.isNull())
        findIcon = QIcon::fromTheme(QStringLiteral("edit-find"), QIcon::fromTheme(QStringLiteral("system-search")));
    if (!findIcon.isNull())
        m_search->addAction(findIcon, QLineEdit::LeadingPosition);
    m_search->installEventFilter(this);

    auto *menuBtn = new QToolButton;
    menuBtn->setIcon(QIcon::fromTheme(QStringLiteral("open-menu-symbolic"),
                                      QIcon::fromTheme(QStringLiteral("application-menu"))));
    if (menuBtn->icon().isNull())
        menuBtn->setText(QStringLiteral("☰"));
    menuBtn->setAutoRaise(true);
    menuBtn->setFocusPolicy(Qt::TabFocus);
    menuBtn->setToolTip(tr("Main menu"));
    auto *menu = new QMenu(menuBtn);
    menu->addAction(QIcon::fromTheme(QStringLiteral("view-refresh")), tr("Refresh list"),
                    QKeySequence::Refresh, this, [this] {
                        Browsers::available(true);
                        reload();
                    });
    menu->addAction(QIcon::fromTheme(QStringLiteral("folder-open")), tr("Open launchers folder"),
                    this, [] { QDesktopServices::openUrl(QUrl::fromLocalFile(WebApp::applicationsDir())); });
    // ---- выбор языка -----------------------------------------------------------
    QMenu *langMenu = menu->addMenu(QIcon::fromTheme(QStringLiteral("preferences-desktop-locale")),
                                    tr("Language"));
    auto *langGroup = new QActionGroup(langMenu);
    const QString saved = I18n::savedLanguage();
    auto addLang = [&](const QString &code, const QString &title) {
        QAction *a = langMenu->addAction(title);
        a->setCheckable(true);
        a->setChecked(code == saved);
        a->setData(code);
        langGroup->addAction(a);
    };
    addLang(QString(), tr("System default"));
    langMenu->addSeparator();
    for (const auto &l : I18n::availableLanguages())
        addLang(l.first, l.second);
    connect(langGroup, &QActionGroup::triggered, this, [this](QAction *a) {
        const QString code = a->data().toString();
        if (code == I18n::savedLanguage())
            return;
        I18n::saveLanguage(code);
        I18n::apply(code);
        QSettings().setValue(QStringLiteral("geometry"), saveGeometry());
        // Пересоздание окна — после выхода из обработчика меню.
        QMetaObject::invokeMethod(this, &MainWindow::languageChanged, Qt::QueuedConnection);
    });

    menu->addSeparator();
    menu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), tr("Quit"),
                    QKeySequence::Quit, qApp, &QApplication::quit);
    // Меню открываем сами, а не через setMenu(): так у кнопки нет стрелочки
    // и не нужен styleSheet, который ломает вид в Breeze/Kvantum.
    connect(menuBtn, &QToolButton::clicked, this, [menuBtn, menu] {
        menuBtn->setDown(true);
        QPoint pos = menuBtn->mapToGlobal(QPoint(menuBtn->width(), menuBtn->height()));
        pos.rx() -= menu->sizeHint().width();
        menu->exec(pos);
        menuBtn->setDown(false);
    });

    m_counter = new QLabel;
    m_counter->setForegroundRole(QPalette::PlaceholderText);

    // ---- группа кнопок действий (слева от поиска) ---------------------------
    m_addBtn = makeButton({QStringLiteral("list-add"), QStringLiteral("list-add-symbolic")}, QStringLiteral("+"),
                          tr("Add (Ctrl+N)"));
    m_removeBtn = makeButton({QStringLiteral("list-remove"), QStringLiteral("list-remove-symbolic")},
                             QStringLiteral("−"), tr("Remove (Delete)"));
    m_editBtn = makeButton({QStringLiteral("document-edit"), QStringLiteral("edit-entry"),
                            QStringLiteral("document-properties"), QStringLiteral("document-edit-symbolic")},
                           QStringLiteral("✎"), tr("Edit (F2)"));
    m_runBtn = makeButton({QStringLiteral("media-playback-start"), QStringLiteral("system-run"),
                           QStringLiteral("media-playback-start-symbolic")},
                          QStringLiteral("▶"), tr("Launch (Enter)"));

    // ---- панель инструментов (как в PCManFM-Qt) ------------------------------
    // Настоящий QToolBar: высоту, отступы и вид кнопок задаёт текущий стиль Qt,
    // поэтому всё выглядит родным в Fusion, Breeze, Kvantum и т.д.
    auto *toolbar = new QToolBar(tr("Main toolbar"), this);
    toolbar->setObjectName(QStringLiteral("mainToolbar"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    const QSize iconSize = toolbar->iconSize(); // размер значков панели из стиля (обычно 22–24)
    for (QToolButton *b : {m_addBtn, m_removeBtn, m_editBtn, m_runBtn, menuBtn}) {
        b->setIconSize(iconSize);
        b->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }

    for (QToolButton *b : {m_addBtn, m_removeBtn, m_editBtn, m_runBtn})
        toolbar->addWidget(b);
    toolbar->addSeparator();

    m_search->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_search->setMinimumWidth(160);
    toolbar->addWidget(m_search);

    auto *counterBox = new QWidget;
    auto *counterLay = new QHBoxLayout(counterBox);
    counterLay->setContentsMargins(6, 0, 2, 0);
    counterLay->addWidget(m_counter);
    toolbar->addWidget(counterBox);
    toolbar->addWidget(menuBtn);

    addToolBar(Qt::TopToolBarArea, toolbar);
    setContextMenuPolicy(Qt::NoContextMenu);

    // ---- список ----------------------------------------------------------------
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

    m_view = new QTreeView;
    m_view->setModel(m_proxy);
    m_view->setRootIsDecorated(false);
    m_view->setUniformRowHeights(true);
    m_view->setAlternatingRowColors(false);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setIconSize(QSize(32, 32));
    m_view->setSortingEnabled(true);
    m_view->sortByColumn(WebAppModel::NameCol, Qt::AscendingOrder);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->installEventFilter(this);
    m_view->setStyleSheet(QStringLiteral("QTreeView::item { padding: 4px 2px; }"));

    QHeaderView *h = m_view->header();
    h->setStretchLastSection(false);
    h->setSectionResizeMode(WebAppModel::IconCol, QHeaderView::Fixed);
    h->resizeSection(WebAppModel::IconCol, 52);
    h->setSectionResizeMode(WebAppModel::NameCol, QHeaderView::Stretch);
    h->setSectionResizeMode(WebAppModel::UrlCol, QHeaderView::Stretch);
    h->setSectionResizeMode(WebAppModel::BrowserCol, QHeaderView::ResizeToContents);

    m_empty = new QLabel;
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);
    m_empty->setForegroundRole(QPalette::PlaceholderText);
    m_empty->setAutoFillBackground(true);
    m_empty->setBackgroundRole(QPalette::Base);
    m_empty->setFrameShape(QFrame::StyledPanel);

    m_stack = new QStackedWidget;
    m_stack->addWidget(m_view);
    m_stack->addWidget(m_empty);

    auto *central = new QWidget;
    auto *lay = new QVBoxLayout(central);
    lay->setContentsMargins(6, 0, 6, 6);
    lay->setSpacing(0);
    lay->addWidget(m_stack, 1);
    setCentralWidget(central);

    // ---- сигналы ----------------------------------------------------------------
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &q) {
        m_proxy->setQuery(q);
        if (!m_view->currentIndex().isValid() && m_proxy->rowCount() > 0)
            m_view->setCurrentIndex(m_proxy->index(0, WebAppModel::NameCol));
        updateState();
    });
    connect(m_view, &QTreeView::activated, this, &MainWindow::launchApp);
    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::updateState);
    connect(m_addBtn, &QToolButton::clicked, this, &MainWindow::addApp);
    connect(m_removeBtn, &QToolButton::clicked, this, &MainWindow::removeApp);
    connect(m_editBtn, &QToolButton::clicked, this, &MainWindow::editApp);
    connect(m_runBtn, &QToolButton::clicked, this, &MainWindow::launchApp);

    connect(m_view, &QTreeView::customContextMenuRequested, this, [this](const QPoint &pos) {
        if (currentSourceRow() < 0)
            return;
        QMenu m(this);
        m.addAction(m_runBtn->icon(), tr("Launch"), this, &MainWindow::launchApp);
        m.addAction(m_editBtn->icon(), tr("Edit…"), this, &MainWindow::editApp);
        m.addAction(QIcon::fromTheme(QStringLiteral("internet-web-browser")), tr("Open in browser"), this, [this] {
            QDesktopServices::openUrl(QUrl(m_model->app(currentSourceRow()).url));
        });
        m.addSeparator();
        m.addAction(m_removeBtn->icon(), tr("Remove…"), this, &MainWindow::removeApp);
        m.exec(m_view->viewport()->mapToGlobal(pos));
    });

    auto *findSc = new QShortcut(QKeySequence::Find, this);
    connect(findSc, &QShortcut::activated, this, [this] { m_search->setFocus(); m_search->selectAll(); });
    new QShortcut(QKeySequence::New, this, this, &MainWindow::addApp);
    new QShortcut(QKeySequence(Qt::Key_F2), this, this, &MainWindow::editApp);
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_E), this, this, &MainWindow::editApp);

    // Следим за папкой с ярлыками: изменения извне (или из Mint) подхватываются сами.
    m_reloadTimer->setSingleShot(true);
    m_reloadTimer->setInterval(300);
    connect(m_reloadTimer, &QTimer::timeout, this, [this] { reload(); });
    QDir().mkpath(WebApp::applicationsDir());
    m_watcher->addPath(WebApp::applicationsDir());
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, m_reloadTimer, qOverload<>(&QTimer::start));

    QSettings s;
    restoreGeometry(s.value(QStringLiteral("geometry")).toByteArray());

    reload();
    m_search->setFocus();
}

QString MainWindow::searchText() const
{
    return m_search->text();
}

void MainWindow::setSearchText(const QString &text)
{
    m_search->setText(text);
    m_search->end(false);
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    QSettings().setValue(QStringLiteral("geometry"), saveGeometry());
    QMainWindow::closeEvent(e);
}

void MainWindow::reload(const QString &selectCode)
{
    QString keep = selectCode;
    if (keep.isEmpty() && currentSourceRow() >= 0)
        keep = m_model->app(currentSourceRow()).codename;
    m_model->setApps(WebApp::loadAll());
    if (!keep.isEmpty())
        selectCodename(keep);
    if (!m_view->currentIndex().isValid() && m_proxy->rowCount() > 0)
        m_view->setCurrentIndex(m_proxy->index(0, WebAppModel::NameCol));
    updateState();
}

void MainWindow::selectCodename(const QString &codename)
{
    const int row = m_model->rowOfCodename(codename);
    if (row < 0)
        return;
    const QModelIndex pi = m_proxy->mapFromSource(m_model->index(row, WebAppModel::NameCol));
    if (pi.isValid()) {
        m_view->setCurrentIndex(pi);
        m_view->scrollTo(pi);
    }
}

int MainWindow::currentSourceRow() const
{
    const QModelIndex i = m_view->currentIndex();
    if (!i.isValid())
        return -1;
    return m_proxy->mapToSource(i).row();
}

void MainWindow::updateState()
{
    const int total = m_model->rowCount();
    const int shown = m_proxy->rowCount();
    const bool has = currentSourceRow() >= 0;
    m_removeBtn->setEnabled(has);
    m_editBtn->setEnabled(has);
    m_runBtn->setEnabled(has);

    if (total == 0) {
        m_empty->setText(tr("No web apps yet.\nClick “+” to add the first one."));
        m_stack->setCurrentWidget(m_empty);
    } else if (shown == 0) {
        m_empty->setText(tr("Nothing found for “%1”.").arg(m_search->text().trimmed()));
        m_stack->setCurrentWidget(m_empty);
    } else {
        m_stack->setCurrentWidget(m_view);
    }
    m_counter->setText(m_proxy->query().trimmed().isEmpty()
                           ? QString::number(total)
                           : tr("%1 of %2").arg(shown).arg(total));
}

bool MainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    if (ev->type() != QEvent::KeyPress)
        return QMainWindow::eventFilter(obj, ev);
    auto *ke = static_cast<QKeyEvent *>(ev);

    if (obj == m_search) {
        switch (ke->key()) {
        case Qt::Key_Down:
        case Qt::Key_Up:
        case Qt::Key_PageDown:
        case Qt::Key_PageUp:
            // Стрелки из строки поиска двигают выделение в списке.
            QApplication::sendEvent(m_view, ev);
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            launchApp();
            return true;
        case Qt::Key_Escape:
            if (!m_search->text().isEmpty()) {
                m_search->clear();
                return true;
            }
            break;
        }
    } else if (obj == m_view) {
        if (ke->key() == Qt::Key_Delete) {
            removeApp();
            return true;
        }
        // Печать в списке сразу уходит в строку поиска.
        const QString t = ke->text();
        if (!t.isEmpty() && t.at(0).isPrint() && !(ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) {
            m_search->setFocus();
            m_search->insert(t);
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::addApp()
{
    WebAppDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        const QString code = dlg.webApp().codename;
        // Новое приложение может не подходить под текущий фильтр — сбросим его.
        if (!m_proxy->query().isEmpty())
            m_search->clear();
        reload(code);
    }
}

void MainWindow::editApp()
{
    const int row = currentSourceRow();
    if (row < 0)
        return;
    WebAppDialog dlg(this);
    dlg.setWebApp(m_model->app(row));
    if (dlg.exec() == QDialog::Accepted)
        reload(dlg.webApp().codename);
}

void MainWindow::removeApp()
{
    const int row = currentSourceRow();
    if (row < 0)
        return;
    const WebApp app = m_model->app(row);

    QMessageBox box(QMessageBox::Question, tr("Remove web app"),
                    tr("Remove “%1”?").arg(app.name), QMessageBox::Yes | QMessageBox::No, this);
    box.setDefaultButton(QMessageBox::No);
    box.button(QMessageBox::Yes)->setText(tr("Remove"));
    box.button(QMessageBox::No)->setText(tr("Cancel"));
    auto *cb = new QCheckBox(tr("Also remove the browser profile (cookies, logins)"));
    cb->setChecked(true);
    box.setCheckBox(cb);
    if (box.exec() != QMessageBox::Yes)
        return;

    // Выделить соседнюю строку после удаления.
    const QModelIndex cur = m_view->currentIndex();
    QModelIndex next = m_proxy->index(cur.row() + 1, WebAppModel::NameCol);
    if (!next.isValid())
        next = m_proxy->index(cur.row() - 1, WebAppModel::NameCol);
    const QString nextCode = next.isValid() ? next.data(WebAppModel::CodenameRole).toString() : QString();

    QString err;
    if (!app.remove(cb->isChecked(), &err))
        QMessageBox::critical(this, windowTitle(), err);
    m_view->setCurrentIndex({});
    reload(nextCode);
}

void MainWindow::launchApp()
{
    const int row = currentSourceRow();
    if (row < 0)
        return;
    QString err;
    if (!m_model->app(row).launch(&err))
        QMessageBox::critical(this, windowTitle(), err);
}
