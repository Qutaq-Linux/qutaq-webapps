#include "webappmodel.h"

#include <QApplication>
#include <QCollator>
#include <QRegularExpression>
#include <QFileInfo>
#include <QStyle>
#include <QUrl>

QIcon iconForWebApp(const QString &icon)
{
    if (!icon.isEmpty() && QFileInfo::exists(icon))
        return QIcon(icon);
    const QIcon fallback = QIcon::fromTheme(QStringLiteral("applications-internet"),
                                            QIcon::fromTheme(QStringLiteral("web-browser"),
                                                             qApp->style()->standardIcon(QStyle::SP_DriveNetIcon)));
    if (icon.isEmpty() || icon == WebApp::defaultIconName()) {
        // Значок по умолчанию = логотип программы: из темы (если установлен),
        // иначе встроенный в бинарник (он же значок окна, см. main.cpp).
        const QIcon themed = QIcon::fromTheme(WebApp::defaultIconName());
        if (!themed.isNull())
            return themed;
        const QIcon app = QApplication::windowIcon();
        return app.isNull() ? fallback : app;
    }
    return QIcon::fromTheme(icon, fallback);
}

void WebAppModel::setApps(const QList<WebApp> &apps)
{
    beginResetModel();
    m_apps = apps;
    m_iconCache.clear();
    endResetModel();
}

int WebAppModel::rowOfCodename(const QString &codename) const
{
    for (int i = 0; i < m_apps.size(); ++i)
        if (m_apps[i].codename == codename)
            return i;
    return -1;
}

int WebAppModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_apps.size();
}

int WebAppModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant WebAppModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_apps.size())
        return {};
    const WebApp &a = m_apps.at(index.row());

    if (role == SearchTextRole) {
        QString cat = a.category;
        for (const auto &c : WebApp::categories())
            if (c.first == a.category)
                cat += QLatin1Char(' ') + c.second;
        return QStringList{a.name, a.description, a.url, a.browser, cat}.join(QLatin1Char('\n')).toLower();
    }
    if (role == CodenameRole)
        return a.codename;

    switch (index.column()) {
    case IconCol:
        if (role == Qt::DecorationRole) {
            auto it = m_iconCache.find(a.icon);
            if (it == m_iconCache.end())
                it = m_iconCache.insert(a.icon, iconForWebApp(a.icon));
            return *it;
        }
        if (role == Qt::ToolTipRole)
            return a.description;
        break;
    case NameCol:
        if (role == Qt::DisplayRole || role == Qt::EditRole)
            return a.name;
        if (role == Qt::ToolTipRole)
            return a.description.isEmpty() ? a.name : a.name + QStringLiteral("\n") + a.description;
        break;
    case UrlCol:
        if (role == Qt::DisplayRole) {
            const QUrl u(a.url);
            return u.host().isEmpty() ? a.url : u.host() + (u.path().size() > 1 ? u.path() : QString());
        }
        if (role == Qt::ToolTipRole || role == Qt::EditRole)
            return a.url;
        break;
    case BrowserCol:
        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            if (!Browsers::find(a.browser))
                return tr("%1 (not installed)").arg(a.browser);
            return a.browser;
        }
        if (role == Qt::ForegroundRole && !Browsers::find(a.browser))
            return QApplication::palette().color(QPalette::Disabled, QPalette::Text);
        break;
    }
    return {};
}

QVariant WebAppModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (o != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case IconCol: return tr("Icon");
    case NameCol: return tr("Name");
    case UrlCol: return tr("Address");
    case BrowserCol: return tr("Browser");
    }
    return {};
}

// ---------------------------------------------------------------------------

void WebAppFilterModel::setQuery(const QString &query)
{
    m_query = query;
    m_terms = query.toLower().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    invalidateFilter();
}

bool WebAppFilterModel::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    if (m_terms.isEmpty())
        return true;
    const QString hay = sourceModel()->index(row, 0, parent).data(WebAppModel::SearchTextRole).toString();
    for (const QString &t : m_terms)
        if (!hay.contains(t))
            return false;
    return true;
}

bool WebAppFilterModel::lessThan(const QModelIndex &l, const QModelIndex &r) const
{
    static QCollator coll = [] {
        QCollator c;
        c.setCaseSensitivity(Qt::CaseInsensitive);
        c.setNumericMode(true);
        return c;
    }();
    // Колонку «Значок» сортируем по имени.
    const int col = l.column() == WebAppModel::IconCol ? WebAppModel::NameCol : l.column();
    const QString a = sourceModel()->index(l.row(), col).data(Qt::EditRole).toString();
    const QString b = sourceModel()->index(r.row(), col).data(Qt::EditRole).toString();
    return coll.compare(a, b) < 0;
}
