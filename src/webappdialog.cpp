#include "webappdialog.h"

#include "faviconfetcher.h"
#include "toggleswitch.h"
#include "webappmodel.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

WebAppDialog::WebAppDialog(QWidget *parent)
    : QDialog(parent), m_fetcher(new FaviconFetcher(this))
{
    setWindowTitle(tr("New Web App"));
    setMinimumWidth(560);

    m_name = new QLineEdit;
    m_description = new QLineEdit;
    m_description->setPlaceholderText(tr("Web App"));
    m_url = new QLineEdit;
    m_url->setPlaceholderText(QStringLiteral("https://www.website.com"));
    m_url->setInputMethodHints(Qt::ImhUrlCharactersOnly | Qt::ImhNoAutoUppercase);

    m_fetchBtn = new QToolButton;
    m_fetchBtn->setIcon(QIcon::fromTheme(QStringLiteral("download"),
                                         QIcon::fromTheme(QStringLiteral("emblem-downloads"))));
    if (m_fetchBtn->icon().isNull())
        m_fetchBtn->setText(QStringLiteral("⤓"));
    m_fetchBtn->setToolTip(tr("Download website icon"));

    auto *urlRow = new QHBoxLayout;
    urlRow->setContentsMargins(0, 0, 0, 0);
    urlRow->addWidget(m_url, 1);
    urlRow->addWidget(m_fetchBtn);

    m_iconBtn = new QToolButton;
    m_iconBtn->setFixedSize(72, 72);
    m_iconBtn->setIconSize(QSize(56, 56));
    m_iconBtn->setPopupMode(QToolButton::InstantPopup);
    m_iconBtn->setToolTip(tr("Choose icon"));
    m_iconBtn->setStyleSheet(QStringLiteral("QToolButton::menu-indicator { image: none; width: 0; }"));
    auto *iconMenu = new QMenu(m_iconBtn);
    m_iconBtn->setMenu(iconMenu);
    connect(iconMenu, &QMenu::aboutToShow, this, [this, iconMenu] {
        iconMenu->clear();
        for (const QImage &img : std::as_const(m_fetched)) {
            QAction *a = iconMenu->addAction(QIcon(QPixmap::fromImage(img)),
                                             tr("Website icon %1×%2").arg(img.width()).arg(img.height()));
            connect(a, &QAction::triggered, this, [this, img] {
                m_iconChosenByUser = true;
                setPendingImage(img);
            });
        }
        if (!m_fetched.isEmpty())
            iconMenu->addSeparator();
        iconMenu->addAction(QIcon::fromTheme(QStringLiteral("document-open")), tr("From file…"),
                            this, &WebAppDialog::chooseIconFile);
        iconMenu->addAction(QIcon::fromTheme(QStringLiteral("preferences-desktop-icons")),
                            tr("From icon theme…"), this, &WebAppDialog::chooseThemeIcon);
        iconMenu->addAction(QIcon::fromTheme(QStringLiteral("view-refresh")),
                            tr("Download from website"), this, &WebAppDialog::fetchFavicon);
        iconMenu->addAction(tr("Default"), this, [this] {
            m_iconChosenByUser = true;
            setIconValue({});
        });
    });

    m_category = new QComboBox;
    for (const auto &c : WebApp::categories())
        m_category->addItem(c.second, c.first);

    m_browser = new QComboBox;
    for (const Browser &b : Browsers::available()) {
        const QString iconName = b.kind == Browser::Firefox ? QStringLiteral("firefox")
                                                            : QStringLiteral("chromium");
        m_browser->addItem(QIcon::fromTheme(iconName), b.name, b.name);
    }
    const QString lastBrowser = QSettings().value(QStringLiteral("lastBrowser")).toString();
    if (int i = m_browser->findData(lastBrowser); i >= 0)
        m_browser->setCurrentIndex(i);

    m_category->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_browser->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_params = new QLineEdit;
    m_params->setPlaceholderText(tr("Custom browser parameters"));

    m_navbar = new ToggleSwitch;
    m_private = new ToggleSwitch;
    m_isolated = new ToggleSwitch;
    m_isolated->setChecked(true);
    m_isolatedLabel = new QLabel(tr("Isolated profile:"));

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    form->addRow(tr("Name:"), m_name);
    form->addRow(tr("Description:"), m_description);
    form->addRow(tr("Address:"), urlRow);
    form->addRow(tr("Icon:"), m_iconBtn);
    form->addRow(tr("Category:"), m_category);
    form->addRow(tr("Browser:"), m_browser);
    form->addRow(tr("Custom parameters:"), m_params);
    form->addRow(tr("Navigation bar:"), m_navbar);
    form->addRow(tr("Private/Incognito window:"), m_private);
    form->addRow(m_isolatedLabel, m_isolated);

    m_status = new QLabel;
    m_status->setWordWrap(true);
    QFont f = m_status->font();
    f.setPointSizeF(f.pointSizeF() * 0.9);
    m_status->setFont(f);
    m_status->setForegroundRole(QPalette::PlaceholderText);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    // Свои подписи: не для всех языков у Qt есть перевод стандартных кнопок.
    m_ok->setText(tr("OK"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    buttons->setCenterButtons(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &WebAppDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &WebAppDialog::reject);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 20, 24, 16);
    lay->addLayout(form);
    lay->addWidget(m_status);
    lay->addStretch();
    lay->addWidget(buttons);

    if (Browsers::available().isEmpty())
        m_status->setText(tr("No supported browsers were found on this system "
                             "(Firefox, Chromium, Chrome, Brave, Vivaldi, Edge…)."));

    connect(m_name, &QLineEdit::textChanged, this, &WebAppDialog::validate);
    connect(m_url, &QLineEdit::textChanged, this, &WebAppDialog::validate);
    connect(m_url, &QLineEdit::editingFinished, this, [this] {
        normalizeUrl();
        // Автоматически подтягиваем значок, если пользователь его ещё не выбирал.
        if (!m_iconChosenByUser && m_fetched.isEmpty() && !m_fetcher->isRunning()
            && QUrl(m_url->text()).isValid() && !m_url->text().isEmpty())
            fetchFavicon();
    });
    connect(m_fetchBtn, &QToolButton::clicked, this, &WebAppDialog::fetchFavicon);
    connect(m_browser, &QComboBox::currentIndexChanged, this, [this] {
        updateBrowserDependent();
        validate();
    });
    connect(m_fetcher, &FaviconFetcher::finished, this, &WebAppDialog::onFaviconsFetched);

    setIconValue({});
    updateBrowserDependent();
    validate();
    m_name->setFocus();
}

void WebAppDialog::setWebApp(const WebApp &app)
{
    m_app = app;
    m_editing = true;
    m_iconChosenByUser = true;
    setWindowTitle(tr("Edit Web App"));

    m_name->setText(app.name);
    m_description->setText(app.description);
    m_url->setText(app.url);
    setIconValue(app.icon);
    if (int i = m_category->findData(app.category); i >= 0)
        m_category->setCurrentIndex(i);
    else {
        m_category->addItem(app.category, app.category);
        m_category->setCurrentIndex(m_category->count() - 1);
    }
    int bi = m_browser->findData(app.browser);
    if (bi < 0) {
        m_browser->addItem(tr("%1 (not installed)").arg(app.browser), app.browser);
        bi = m_browser->count() - 1;
    }
    m_browser->setCurrentIndex(bi);
    m_params->setText(app.customParams);
    m_navbar->setChecked(app.navbar);
    m_private->setChecked(app.privateWindow);
    m_isolated->setChecked(app.isolated);
    updateBrowserDependent();
    validate();
}

void WebAppDialog::updateBrowserDependent()
{
    const Browser *b = Browsers::find(m_browser->currentData().toString());
    // У Firefox-подобных браузеров профиль всегда отдельный — переключатель не нужен.
    const bool chromium = b && b->kind == Browser::Chromium;
    m_isolated->setVisible(chromium);
    m_isolatedLabel->setVisible(chromium);
}

void WebAppDialog::normalizeUrl()
{
    QString u = m_url->text().trimmed();
    if (!u.isEmpty() && !u.contains(QLatin1String("://")))
        u.prepend(QStringLiteral("https://"));
    if (u != m_url->text())
        m_url->setText(u);
}

void WebAppDialog::validate()
{
    const QString u = m_url->text().trimmed();
    QUrl url = QUrl::fromUserInput(u);
    const bool urlOk = !u.isEmpty() && url.isValid() && !url.host().isEmpty()
                       && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"));
    const bool browserOk = Browsers::find(m_browser->currentData().toString()) != nullptr;
    m_ok->setEnabled(!m_name->text().trimmed().isEmpty() && urlOk && browserOk);
    m_fetchBtn->setEnabled(urlOk && !m_fetcher->isRunning());
}

void WebAppDialog::fetchFavicon()
{
    normalizeUrl();
    const QUrl url(m_url->text());
    if (!url.isValid() || url.host().isEmpty())
        return;
    m_fetchBtn->setEnabled(false);
    m_status->setText(tr("Downloading icon from %1…").arg(url.host()));
    m_fetcher->fetch(url);
}

void WebAppDialog::onFaviconsFetched(const QList<QImage> &icons, const QString &title)
{
    m_fetched = icons;
    validate();
    if (m_name->text().trimmed().isEmpty() && !title.isEmpty()) {
        // «Почта — Сервис» → «Почта»: берём самую содержательную часть заголовка.
        QString t = title;
        for (const QString &sep : {QStringLiteral(" | "), QStringLiteral(" — "), QStringLiteral(" - "), QStringLiteral(" · ")})
            if (t.contains(sep)) { t = t.section(sep, 0, 0).trimmed(); break; }
        m_name->setText(t.left(60));
    }
    if (icons.isEmpty()) {
        m_status->setText(tr("No website icon found — choose one manually."));
        return;
    }
    m_status->setText(icons.size() == 1
                          ? tr("Website icon found.")
                          : tr("Found %n icons. Click the icon to choose another one.", nullptr, int(icons.size())));
    if (!m_iconChosenByUser || !m_pendingImage.isNull())
        setPendingImage(icons.first());
}

void WebAppDialog::setPendingImage(const QImage &img)
{
    m_pendingImage = img;
    m_iconValue.clear();
    m_iconBtn->setIcon(QIcon(QPixmap::fromImage(img)));
}

void WebAppDialog::setIconValue(const QString &icon)
{
    m_pendingImage = QImage();
    m_iconValue = icon;
    m_iconBtn->setIcon(iconForWebApp(icon));
}

void WebAppDialog::chooseIconFile()
{
    QStringList filters;
    for (const QByteArray &fmt : QImageReader::supportedImageFormats())
        filters << QStringLiteral("*.") + QString::fromLatin1(fmt);
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Choose an icon"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        tr("Images (%1)").arg(filters.join(QLatin1Char(' '))));
    if (path.isEmpty())
        return;
    QImage img(path);
    if (img.isNull()) {
        // SVG без QtSvg и т.п. — используем путь как есть, меню сам разберётся.
        m_iconChosenByUser = true;
        setIconValue(path);
        return;
    }
    m_iconChosenByUser = true;
    setPendingImage(img);
}

void WebAppDialog::chooseThemeIcon()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Theme icon"),
                                               tr("Icon name (e.g. mail-client, telegram, discord):"),
                                               QLineEdit::Normal,
                                               QFileInfo(m_iconValue).isAbsolute() ? QString() : m_iconValue,
                                               &ok).trimmed();
    if (!ok || name.isEmpty())
        return;
    if (!QIcon::hasThemeIcon(name))
        QMessageBox::information(this, windowTitle(),
                                 tr("Icon “%1” is not in the current theme, but it will be saved — "
                                    "it will appear if a theme provides it.").arg(name));
    m_iconChosenByUser = true;
    setIconValue(name);
}

void WebAppDialog::accept()
{
    normalizeUrl();
    m_app.name = m_name->text().trimmed();
    m_app.description = m_description->text().trimmed();
    m_app.url = m_url->text().trimmed();
    m_app.category = m_category->currentData().toString();
    m_app.browser = m_browser->currentData().toString();
    m_app.customParams = m_params->text().trimmed();
    m_app.navbar = m_navbar->isChecked();
    m_app.privateWindow = m_private->isChecked();
    m_app.isolated = m_isolated->isChecked();
    if (m_app.codename.isEmpty())
        m_app.codename = WebApp::newCodename(m_app.name);

    if (!m_pendingImage.isNull()) {
        QDir().mkpath(WebApp::iconsDir());
        const QString path = WebApp::iconsDir() + QLatin1Char('/') + m_app.codename + QStringLiteral(".png");
        QImage img = m_pendingImage;
        if (img.width() > 256 || img.height() > 256)
            img = img.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (!img.save(path, "PNG")) {
            QMessageBox::warning(this, windowTitle(), tr("Could not save the icon to %1").arg(path));
            return;
        }
        m_app.icon = path;
    } else {
        m_app.icon = m_iconValue;
    }

    QString err;
    if (!m_app.save(&err)) {
        QMessageBox::critical(this, windowTitle(), err);
        return;
    }
    QSettings().setValue(QStringLiteral("lastBrowser"), m_app.browser);
    QDialog::accept();
}
