#pragma once

#include <QList>
#include <QString>
#include <QStringList>

struct Browser
{
    enum Kind { Firefox, Chromium };

    QString name;        // отображаемое и сохраняемое имя (совместимо с Mint: "Firefox", "Chromium"…)
    Kind kind = Firefox;
    QStringList command; // {"firefox"} или {"flatpak","run","org.mozilla.firefox"}
    QString flatpakId;   // пусто для обычной установки

    bool isFlatpak() const { return !flatpakId.isEmpty(); }
};

namespace Browsers {
// Список браузеров, найденных в системе (кэшируется; reload=true — пересканировать).
const QList<Browser> &available(bool reload = false);
// Поиск по имени среди найденных; nullptr, если браузер не установлен.
const Browser *find(const QString &name);
}
