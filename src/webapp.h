#pragma once

#include "browsers.h"

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

// Веб-приложение. Хранится как ~/.local/share/applications/webapp-<codename>.desktop
// с ключами X-WebApp-* — тот же формат, что у Linux Mint Web App Manager,
// поэтому приложения, созданные в Mint, видны здесь и наоборот.
struct WebApp
{
    QString codename;  // уникальный идентификатор (часть имени файла и WM_CLASS / app_id)
    QString name;
    QString description;
    QString url;
    QString icon;      // путь к файлу или имя значка темы
    QString category = QStringLiteral("WebApps");
    QString browser;   // Browser::name
    QString customParams;
    bool navbar = false;
    bool privateWindow = false;
    bool isolated = true; // отдельный профиль (для Chromium-браузеров)
    QString filePath;

    QString wmClass() const { return QStringLiteral("WebApp-") + codename; }
    QString profileDir(const Browser &b) const;
    QStringList commandLine(const Browser &b) const; // argv, включая программу

    bool launch(QString *error) const;
    bool save(QString *error);                       // записать .desktop (+ профиль Firefox)
    bool remove(bool removeProfile, QString *error) const;

    static bool load(const QString &path, WebApp *out);
    static QList<WebApp> loadAll();
    static QString newCodename(const QString &name);

    static QString applicationsDir();
    static QString iconsDir();
    static QString defaultIconName();

    // (ключ Categories, отображаемое имя)
    static QList<QPair<QString, QString>> categories();
};
