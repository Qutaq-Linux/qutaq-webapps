#include "webapp.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>

namespace {

const QString kPrefix = QStringLiteral("webapp-");
const QString kSuffix = QStringLiteral(".desktop");

// ---- экранирование по Desktop Entry Specification -------------------------

QString escapeValue(const QString &s)
{
    QString r;
    r.reserve(s.size());
    for (QChar c : s) {
        switch (c.unicode()) {
        case '\\': r += QLatin1String("\\\\"); break;
        case '\n': r += QLatin1String("\\n"); break;
        case '\t': r += QLatin1String("\\t"); break;
        case '\r': r += QLatin1String("\\r"); break;
        default: r += c;
        }
    }
    return r;
}

QString unescapeValue(const QString &s)
{
    QString r;
    r.reserve(s.size());
    for (int i = 0; i < s.size(); ++i) {
        if (s[i] == QLatin1Char('\\') && i + 1 < s.size()) {
            const QChar n = s[++i];
            switch (n.unicode()) {
            case 's': r += QLatin1Char(' '); break;
            case 'n': r += QLatin1Char('\n'); break;
            case 't': r += QLatin1Char('\t'); break;
            case 'r': r += QLatin1Char('\r'); break;
            case '\\': r += QLatin1Char('\\'); break;
            default: r += QLatin1Char('\\'); r += n;
            }
        } else {
            r += s[i];
        }
    }
    return r;
}

// Один аргумент Exec: кавычки при необходимости, %→%%.
QString quoteExecArg(const QString &arg)
{
    static const QString reserved = QStringLiteral(" \t\n\"'\\><~|&;$*?#()`");
    bool needQuotes = arg.isEmpty();
    for (QChar c : arg)
        if (reserved.contains(c)) { needQuotes = true; break; }

    QString a = arg;
    if (needQuotes) {
        QString q;
        for (QChar c : arg) {
            if (c == QLatin1Char('"') || c == QLatin1Char('`') || c == QLatin1Char('$')
                || c == QLatin1Char('\\'))
                q += QLatin1Char('\\');
            q += c;
        }
        a = QLatin1Char('"') + q + QLatin1Char('"');
    }
    a.replace(QLatin1Char('%'), QLatin1String("%%"));
    return a;
}

// Разбор значения Exec (уже без экранирования уровня значения) в argv.
QStringList splitExec(const QString &exec)
{
    QStringList out;
    QString cur;
    bool inQuotes = false, hasToken = false;
    for (int i = 0; i < exec.size(); ++i) {
        const QChar c = exec[i];
        if (inQuotes) {
            if (c == QLatin1Char('\\') && i + 1 < exec.size()) {
                cur += exec[++i];
            } else if (c == QLatin1Char('"')) {
                inQuotes = false;
            } else {
                cur += c;
            }
        } else if (c == QLatin1Char('"')) {
            inQuotes = true;
            hasToken = true;
        } else if (c.isSpace()) {
            if (hasToken) out << cur;
            cur.clear();
            hasToken = false;
        } else if (c == QLatin1Char('%') && i + 1 < exec.size()) {
            const QChar n = exec[++i];
            if (n == QLatin1Char('%')) { cur += n; hasToken = true; }
            // остальные коды полей (%u, %U, %f…) просто отбрасываем
        } else {
            cur += c;
            hasToken = true;
        }
    }
    if (hasToken) out << cur;
    return out;
}

bool toBool(const QString &s) { return s.trimmed().compare(QLatin1String("true"), Qt::CaseInsensitive) == 0; }
QString fromBool(bool b) { return b ? QStringLiteral("true") : QStringLiteral("false"); }

bool writeText(const QString &path, const QString &text, QString *error)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(text.toUtf8());
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

QString dataHome()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
}

// Профиль Firefox: скрываем панели через userChrome.css.
bool writeFirefoxProfile(const QString &dir, bool navbar, QString *error)
{
    if (!QDir().mkpath(dir + QStringLiteral("/chrome"))) {
        if (error) *error = QCoreApplication::translate("WebApp", "Could not create profile directory %1").arg(dir);
        return false;
    }
    const QString userJs = QStringLiteral(
        "// Created by Qutaq Web App Manager\n"
        "user_pref(\"toolkit.legacyUserProfileCustomizations.stylesheets\", true);\n"
        "user_pref(\"browser.shell.checkDefaultBrowser\", false);\n"
        "user_pref(\"browser.startup.homepage_override.mstone\", \"ignore\");\n"
        "user_pref(\"browser.aboutwelcome.enabled\", false);\n"
        "user_pref(\"browser.tabs.warnOnClose\", false);\n"
        "user_pref(\"browser.sessionstore.resume_from_crash\", false);\n"
        "user_pref(\"datareporting.policy.dataSubmissionPolicyBypassNotification\", true);\n"
        "user_pref(\"browser.tabs.inTitlebar\", 0);\n"
        "user_pref(\"widget.use-xdg-desktop-portal.file-picker\", 1);\n");

    QString css = QStringLiteral("/* Created by Qutaq Web App Manager — do not edit manually */\n"
                                 "#TabsToolbar { visibility: collapse !important; }\n"
                                 "#sidebar-header, #sidebar-box { display: none !important; }\n");
    if (!navbar)
        css += QStringLiteral("#nav-bar, #PersonalToolbar, #titlebar { visibility: collapse !important; }\n");

    return writeText(dir + QStringLiteral("/user.js"), userJs, error)
           && writeText(dir + QStringLiteral("/chrome/userChrome.css"), css, error);
}

} // namespace

// ---------------------------------------------------------------------------

QString WebApp::applicationsDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
}

QString WebApp::iconsDir()
{
    return dataHome() + QStringLiteral("/ice/icons");
}

QString WebApp::defaultIconName()
{
    // Логотип самой программы (data/icons, см. QUTAQ_ICON в CMakeLists.txt).
    return QStringLiteral(APP_ID);
}

QList<QPair<QString, QString>> WebApp::categories()
{
    // Не static: названия должны переводиться при смене языка.
    return {
        {QStringLiteral("WebApps"), QCoreApplication::translate("WebApp", "Web")},
        {QStringLiteral("Network"), QCoreApplication::translate("WebApp", "Internet")},
        {QStringLiteral("Office"), QCoreApplication::translate("WebApp", "Office")},
        {QStringLiteral("Development"), QCoreApplication::translate("WebApp", "Programming")},
        {QStringLiteral("Graphics"), QCoreApplication::translate("WebApp", "Graphics")},
        {QStringLiteral("AudioVideo"), QCoreApplication::translate("WebApp", "Sound & Video")},
        {QStringLiteral("Game"), QCoreApplication::translate("WebApp", "Games")},
        {QStringLiteral("Education"), QCoreApplication::translate("WebApp", "Education")},
        {QStringLiteral("Utility"), QCoreApplication::translate("WebApp", "Accessories")},
    };
}

QString WebApp::newCodename(const QString &name)
{
    QString base;
    for (QChar c : name)
        if (c.isLetterOrNumber() && c.unicode() < 128)
            base += c;
    if (base.isEmpty())
        base = QStringLiteral("App");
    base = base.left(24);

    const QString dir = applicationsDir();
    QString code;
    do {
        code = base + QString::number(QRandomGenerator::global()->bounded(1000, 10000));
    } while (QFileInfo::exists(dir + QLatin1Char('/') + kPrefix + code + kSuffix));
    return code;
}

QString WebApp::profileDir(const Browser &b) const
{
    const QString base = b.isFlatpak()
        ? QDir::homePath() + QStringLiteral("/.var/app/") + b.flatpakId + QStringLiteral("/data/ice")
        : dataHome() + QStringLiteral("/ice");
    return base + (b.kind == Browser::Firefox ? QStringLiteral("/firefox/")
                                              : QStringLiteral("/profiles/"))
           + codename;
}

QStringList WebApp::commandLine(const Browser &b) const
{
    QStringList args = b.command;
    const QStringList extra = QProcess::splitCommand(customParams);

    if (b.kind == Browser::Firefox) {
        // --name задаёт app_id окна на Wayland, --class — WM_CLASS на X11.
        args << QStringLiteral("--class") << wmClass()
             << QStringLiteral("--name") << wmClass()
             << QStringLiteral("--profile") << profileDir(b)
             << QStringLiteral("--no-remote");
        if (privateWindow)
            args << QStringLiteral("--private-window");
        args << extra << url;
    } else {
        args << QStringLiteral("--class=") + wmClass();
        if (isolated)
            args << QStringLiteral("--user-data-dir=") + profileDir(b);
        if (privateWindow)
            args << QStringLiteral("--incognito");
        args << extra;
        if (navbar)
            args << QStringLiteral("--new-window") << url;
        else
            args << QStringLiteral("--app=") + url;
    }
    return args;
}

bool WebApp::save(QString *error)
{
    const Browser *b = Browsers::find(browser);
    if (!b) {
        if (error) *error = QCoreApplication::translate("WebApp", "Browser “%1” was not found on this system.").arg(browser);
        return false;
    }
    if (codename.isEmpty())
        codename = newCodename(name);

    const QString dir = applicationsDir();
    QDir().mkpath(dir);
    if (filePath.isEmpty())
        filePath = dir + QLatin1Char('/') + kPrefix + codename + kSuffix;

    if (b->kind == Browser::Firefox && !writeFirefoxProfile(profileDir(*b), navbar, error))
        return false;
    if (b->kind == Browser::Chromium && isolated)
        QDir().mkpath(profileDir(*b));

    QStringList execParts;
    for (const QString &a : commandLine(*b))
        execParts << quoteExecArg(a);

    QString t;
    QTextStream s(&t);
    s << "[Desktop Entry]\n"
      << "Version=1.0\n"
      << "Type=Application\n"
      << "Name=" << escapeValue(name) << '\n'
      << "Comment=" << escapeValue(description.isEmpty() ? QCoreApplication::translate("WebApp", "Web App") : description) << '\n'
      << "Exec=" << escapeValue(execParts.join(QLatin1Char(' '))) << '\n'
      << "Terminal=false\n"
      << "X-MultipleArgs=false\n"
      << "Icon=" << escapeValue(icon.isEmpty() ? defaultIconName() : icon) << '\n'
      << "Categories=GTK;" << category << ";\n"
      << "StartupWMClass=" << wmClass() << '\n'
      << "StartupNotify=true\n"
      << "X-WebApp-Browser=" << escapeValue(browser) << '\n'
      << "X-WebApp-URL=" << escapeValue(url) << '\n'
      << "X-WebApp-CustomParameters=" << escapeValue(customParams) << '\n'
      << "X-WebApp-Navbar=" << fromBool(navbar) << '\n'
      << "X-WebApp-PrivateWindow=" << fromBool(privateWindow) << '\n'
      << "X-WebApp-Isolated=" << fromBool(isolated) << '\n';
    s.flush();

    if (!writeText(filePath, t, error))
        return false;
    QFile::setPermissions(filePath, QFile::permissions(filePath) | QFileDevice::ExeOwner);

    // Обновить кэш меню (не обязательно, но многие окружения это любят).
    const QString udd = QStandardPaths::findExecutable(QStringLiteral("update-desktop-database"));
    if (!udd.isEmpty())
        QProcess::startDetached(udd, {dir});
    return true;
}

bool WebApp::launch(QString *error) const
{
    QStringList argv;
    if (const Browser *b = Browsers::find(browser)) {
        argv = commandLine(*b);
    } else {
        // Браузер не опознан — пробуем строку Exec из самого .desktop-файла.
        QFile f(filePath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&f);
            while (!in.atEnd()) {
                const QString line = in.readLine();
                if (line.startsWith(QLatin1String("Exec="))) {
                    argv = splitExec(unescapeValue(line.mid(5)));
                    break;
                }
            }
        }
    }
    if (argv.isEmpty()) {
        if (error) *error = QCoreApplication::translate("WebApp", "Could not determine the launch command.");
        return false;
    }
    const QString program = argv.takeFirst();
    if (!QProcess::startDetached(program, argv)) {
        if (error) *error = QCoreApplication::translate("WebApp", "Could not start %1").arg(program);
        return false;
    }
    return true;
}

bool WebApp::remove(bool removeProfile, QString *error) const
{
    if (!filePath.isEmpty() && QFileInfo::exists(filePath) && !QFile::remove(filePath)) {
        if (error) *error = QCoreApplication::translate("WebApp", "Could not remove %1").arg(filePath);
        return false;
    }
    if (removeProfile && !codename.isEmpty()) {
        for (const Browser &b : Browsers::available())
            if (b.name == browser)
                QDir(profileDir(b)).removeRecursively();
        // Профили, созданные под другим браузером того же типа (если браузер меняли).
        QDir(dataHome() + QStringLiteral("/ice/firefox/") + codename).removeRecursively();
        QDir(dataHome() + QStringLiteral("/ice/profiles/") + codename).removeRecursively();
    }
    // Значок удаляем только если он лежит в нашем каталоге.
    if (icon.startsWith(iconsDir() + QLatin1Char('/')))
        QFile::remove(icon);
    return true;
}

bool WebApp::load(const QString &path, WebApp *out)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QHash<QString, QString> kv;
    bool inMain = false;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1Char('['))) {
            inMain = (line == QLatin1String("[Desktop Entry]"));
            continue;
        }
        if (!inMain)
            continue;
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed();
        if (!kv.contains(key))
            kv.insert(key, unescapeValue(line.mid(eq + 1).trimmed()));
    }

    if (!kv.contains(QStringLiteral("X-WebApp-URL")))
        return false; // не наше веб-приложение

    WebApp a;
    a.filePath = path;
    QString fname = QFileInfo(path).fileName();
    a.codename = fname.mid(kPrefix.size(), fname.size() - kPrefix.size() - kSuffix.size());
    a.name = kv.value(QStringLiteral("Name"));
    a.description = kv.value(QStringLiteral("Comment"));
    a.url = kv.value(QStringLiteral("X-WebApp-URL"));
    a.icon = kv.value(QStringLiteral("Icon"));
    a.browser = kv.value(QStringLiteral("X-WebApp-Browser"));
    a.customParams = kv.value(QStringLiteral("X-WebApp-CustomParameters"));
    a.navbar = toBool(kv.value(QStringLiteral("X-WebApp-Navbar")));
    a.privateWindow = toBool(kv.value(QStringLiteral("X-WebApp-PrivateWindow")));
    a.isolated = kv.contains(QStringLiteral("X-WebApp-Isolated"))
                     ? toBool(kv.value(QStringLiteral("X-WebApp-Isolated")))
                     : true;

    const QStringList cats = kv.value(QStringLiteral("Categories")).split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &c : cats) {
        if (c != QLatin1String("GTK") && c != QLatin1String("Qt")) {
            a.category = c;
            break;
        }
    }
    *out = a;
    return true;
}

QList<WebApp> WebApp::loadAll()
{
    QList<WebApp> list;
    const QDir dir(applicationsDir());
    const QStringList files = dir.entryList({kPrefix + QLatin1Char('*') + kSuffix}, QDir::Files);
    for (const QString &f : files) {
        WebApp a;
        if (load(dir.filePath(f), &a))
            list << a;
    }
    return list;
}
