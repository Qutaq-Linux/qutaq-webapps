#include "browsers.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>

namespace {

struct Candidate
{
    const char *name;
    Browser::Kind kind;
    QStringList binaries; // проверяются по порядку
};

struct FlatpakCandidate
{
    const char *name;
    Browser::Kind kind;
    const char *id;
};

bool flatpakInstalled(const QString &id)
{
    const QStringList roots = {
        QStringLiteral("/var/lib/flatpak/app/"),
        QDir::homePath() + QStringLiteral("/.local/share/flatpak/app/"),
    };
    for (const QString &r : roots)
        if (QFileInfo::exists(r + id))
            return true;
    return false;
}

QList<Browser> scan()
{
    using K = Browser::Kind;
    const QList<Candidate> native = {
        {"Firefox", K::Firefox, {"firefox"}},
        {"Firefox ESR", K::Firefox, {"firefox-esr"}},
        {"Firefox Developer Edition", K::Firefox, {"firefox-developer-edition"}},
        {"LibreWolf", K::Firefox, {"librewolf"}},
        {"Waterfox", K::Firefox, {"waterfox"}},
        {"Floorp", K::Firefox, {"floorp"}},
        {"Zen Browser", K::Firefox, {"zen-browser", "zen"}},
        {"Chromium", K::Chromium, {"chromium", "chromium-browser"}},
        {"Ungoogled Chromium", K::Chromium, {"ungoogled-chromium"}},
        {"Google Chrome", K::Chromium, {"google-chrome-stable", "google-chrome"}},
        {"Brave", K::Chromium, {"brave-browser", "brave"}},
        {"Vivaldi", K::Chromium, {"vivaldi-stable", "vivaldi"}},
        {"Microsoft Edge", K::Chromium, {"microsoft-edge-stable", "microsoft-edge"}},
        {"Yandex Browser", K::Chromium, {"yandex-browser", "yandex-browser-stable"}},
        {"Thorium", K::Chromium, {"thorium-browser"}},
    };
    const QList<FlatpakCandidate> flatpaks = {
        {"Firefox (Flatpak)", K::Firefox, "org.mozilla.firefox"},
        {"LibreWolf (Flatpak)", K::Firefox, "io.gitlab.librewolf-community"},
        {"Zen Browser (Flatpak)", K::Firefox, "app.zen_browser.zen"},
        {"Floorp (Flatpak)", K::Firefox, "one.ablaze.floorp"},
        {"Chromium (Flatpak)", K::Chromium, "org.chromium.Chromium"},
        {"Ungoogled Chromium (Flatpak)", K::Chromium, "io.github.ungoogled_software.ungoogled_chromium"},
        {"Google Chrome (Flatpak)", K::Chromium, "com.google.Chrome"},
        {"Brave (Flatpak)", K::Chromium, "com.brave.Browser"},
        {"Vivaldi (Flatpak)", K::Chromium, "com.vivaldi.Vivaldi"},
        {"Microsoft Edge (Flatpak)", K::Chromium, "com.microsoft.Edge"},
    };

    QList<Browser> out;
    for (const Candidate &c : native) {
        for (const QString &bin : c.binaries) {
            const QString path = QStandardPaths::findExecutable(bin);
            if (!path.isEmpty()) {
                out.append({QString::fromLatin1(c.name), c.kind, {path}, {}});
                break;
            }
        }
    }
    const QString flatpak = QStandardPaths::findExecutable(QStringLiteral("flatpak"));
    if (!flatpak.isEmpty()) {
        for (const FlatpakCandidate &c : flatpaks) {
            const QString id = QString::fromLatin1(c.id);
            if (flatpakInstalled(id))
                out.append({QString::fromLatin1(c.name), c.kind,
                            {flatpak, QStringLiteral("run"), id}, id});
        }
    }
    return out;
}

} // namespace

const QList<Browser> &Browsers::available(bool reload)
{
    static QList<Browser> cache = scan();
    if (reload)
        cache = scan();
    return cache;
}

const Browser *Browsers::find(const QString &name)
{
    for (const Browser &b : available())
        if (b.name.compare(name, Qt::CaseInsensitive) == 0)
            return &b;
    return nullptr;
}
