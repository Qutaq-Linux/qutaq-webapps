#include "i18n.h"

#include <QCoreApplication>
#include <QDir>
#include <QHash>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace {

const QString kQmDir = QStringLiteral(":/i18n");
const QString kPrefix = QStringLiteral(APP_BINARY "_");

QTranslator *g_app = nullptr;
QTranslator *g_qt = nullptr;
QString g_current = QStringLiteral("en");

QString capitalized(QString s)
{
    if (!s.isEmpty())
        s[0] = s[0].toUpper();
    return s;
}

// Короткие самоназвания; для неизвестных кодов — из QLocale.
QString nativeName(const QString &code)
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("ru"), QStringLiteral("Русский")},
        {QStringLiteral("uk"), QStringLiteral("Українська")},
        {QStringLiteral("be"), QStringLiteral("Беларуская")},
        {QStringLiteral("ro"), QStringLiteral("Română")},
        {QStringLiteral("de"), QStringLiteral("Deutsch")},
        {QStringLiteral("fr"), QStringLiteral("Français")},
        {QStringLiteral("es"), QStringLiteral("Español")},
        {QStringLiteral("it"), QStringLiteral("Italiano")},
        {QStringLiteral("pl"), QStringLiteral("Polski")},
        {QStringLiteral("pt_BR"), QStringLiteral("Português (Brasil)")},
        {QStringLiteral("tr"), QStringLiteral("Türkçe")},
        {QStringLiteral("zh_CN"), QStringLiteral("简体中文")},
        {QStringLiteral("zh_TW"), QStringLiteral("繁體中文")},
        {QStringLiteral("ja"), QStringLiteral("日本語")},
    };
    if (const auto it = names.constFind(code); it != names.constEnd())
        return *it;
    return capitalized(QLocale(code).nativeLanguageName());
}

} // namespace

QList<QPair<QString, QString>> I18n::availableLanguages()
{
    QList<QPair<QString, QString>> list;
    list.append({QStringLiteral("en"), QStringLiteral("English")});

    const QStringList files = QDir(kQmDir).entryList({kPrefix + QStringLiteral("*.qm")}, QDir::Files, QDir::Name);
    for (const QString &f : files) {
        const QString code = f.mid(kPrefix.size(), f.size() - kPrefix.size() - 3);
        if (code == QLatin1String("en"))
            continue;
        list.append({code, nativeName(code)});
    }
    return list;
}

QString I18n::savedLanguage()
{
    return QSettings().value(QStringLiteral("language")).toString();
}

void I18n::saveLanguage(const QString &code)
{
    QSettings().setValue(QStringLiteral("language"), code);
}

QString I18n::currentLanguage()
{
    return g_current;
}

void I18n::apply(const QString &code)
{
    auto *app = QCoreApplication::instance();
    if (g_app) { app->removeTranslator(g_app); delete g_app; g_app = nullptr; }
    if (g_qt) { app->removeTranslator(g_qt); delete g_qt; g_qt = nullptr; }

    const QLocale locale = code.isEmpty() ? QLocale::system() : QLocale(code);
    g_current = QStringLiteral("en");

    auto *tApp = new QTranslator;
    // load(QLocale, …) перебирает uiLanguages(): ru_RU → ru и т.д.
    if (locale.language() != QLocale::English && tApp->load(locale, QStringLiteral(APP_BINARY), QStringLiteral("_"), kQmDir)) {
        app->installTranslator(tApp);
        g_app = tApp;
        g_current = tApp->language().isEmpty() ? locale.name() : tApp->language();
    } else {
        delete tApp;
    }

    // Стандартные строки Qt (кнопки «Отмена», «Да», диалог выбора файла…).
    auto *tQt = new QTranslator;
    if (tQt->load(locale, QStringLiteral("qtbase"), QStringLiteral("_"),
                  QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app->installTranslator(tQt);
        g_qt = tQt;
    } else {
        delete tQt;
    }

    QLocale::setDefault(locale);
}
