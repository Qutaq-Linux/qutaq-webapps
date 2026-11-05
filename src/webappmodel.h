#pragma once

#include "webapp.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QIcon>
#include <QSortFilterProxyModel>

QIcon iconForWebApp(const QString &icon);

class WebAppModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { IconCol, NameCol, UrlCol, BrowserCol, ColumnCount };
    enum Role { SearchTextRole = Qt::UserRole + 1, CodenameRole };

    using QAbstractTableModel::QAbstractTableModel;

    void setApps(const QList<WebApp> &apps);
    const WebApp &app(int row) const { return m_apps.at(row); }
    int rowOfCodename(const QString &codename) const;

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;

private:
    QList<WebApp> m_apps;
    mutable QHash<QString, QIcon> m_iconCache;
};

// Фильтр: все слова запроса должны встретиться в имени, описании, адресе,
// категории или браузере (без учёта регистра и порядка слов).
class WebAppFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    void setQuery(const QString &query);
    QString query() const { return m_query; }

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;
    bool lessThan(const QModelIndex &l, const QModelIndex &r) const override;

private:
    QString m_query;
    QStringList m_terms;
};
