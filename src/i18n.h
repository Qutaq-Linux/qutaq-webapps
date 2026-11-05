#pragma once

#include <QList>
#include <QPair>
#include <QString>

namespace I18n {

// Встроенные переводы: (код, название языка на нём самом), английский — первым.
QList<QPair<QString, QString>> availableLanguages();

// Сохранённый выбор: пустая строка = язык системы.
QString savedLanguage();
void saveLanguage(const QString &code);

// Установить переводчики приложения и Qt для кода (пусто = язык системы).
void apply(const QString &code);

// Код фактически используемого языка (после apply).
QString currentLanguage();

} // namespace I18n
