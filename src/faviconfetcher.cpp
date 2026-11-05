#include "faviconfetcher.h"

#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <QUrlQuery>

#ifdef HAVE_QTSVG
#include <QPainter>
#include <QSvgRenderer>
#endif

#include <algorithm>

namespace {

QString attr(const QString &tag, const QString &name)
{
    const QRegularExpression re(
        QStringLiteral("\\b%1\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s>]+))").arg(name),
        QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(tag);
    if (!m.hasMatch())
        return {};
    for (int i = 1; i <= 3; ++i)
        if (!m.captured(i).isNull())
            return QTextDocumentFragment::fromHtml(m.captured(i)).toPlainText().trimmed();
    return {};
}

QImage decodeImage(const QByteArray &data)
{
    QImage img;
    if (img.loadFromData(data))
        return img;
#ifdef HAVE_QTSVG
    QSvgRenderer svg(data);
    if (svg.isValid()) {
        QImage out(256, 256, QImage::Format_ARGB32_Premultiplied);
        out.fill(Qt::transparent);
        QPainter p(&out);
        svg.render(&p);
        return out;
    }
#endif
    return {};
}

} // namespace

FaviconFetcher::FaviconFetcher(QObject *parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this))
{
}

QNetworkReply *FaviconFetcher::get(const QUrl &url)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Mozilla/5.0 (X11; Linux x86_64; rv:128.0) Gecko/20100101 Firefox/128.0"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(10000);
    QNetworkReply *r = m_nam->get(req);
    m_replies << r;
    ++m_pending;
    return r;
}

void FaviconFetcher::abort()
{
    const auto replies = m_replies;
    m_replies.clear();
    for (QNetworkReply *r : replies) {
        r->disconnect(this);
        r->abort();
        r->deleteLater();
    }
    m_pending = 0;
}

void FaviconFetcher::fetch(const QUrl &pageUrl)
{
    abort();
    m_icons.clear();
    m_requested.clear();
    m_hashes.clear();
    m_title.clear();

    QNetworkReply *r = get(pageUrl);
    connect(r, &QNetworkReply::finished, this, [this, r] { onPageReply(r); });
}

void FaviconFetcher::onPageReply(QNetworkReply *reply)
{
    m_replies.removeOne(reply);
    reply->deleteLater();

    const QUrl base = reply->url(); // итоговый адрес после редиректов
    QList<QUrl> candidates;

    if (reply->error() == QNetworkReply::NoError) {
        // Ограничиваемся началом документа — <head> обычно в первых сотнях КБ.
        const QString html = QString::fromUtf8(reply->read(2 * 1024 * 1024));

        static const QRegularExpression titleRe(QStringLiteral("<title[^>]*>(.*?)</title>"),
                                                QRegularExpression::CaseInsensitiveOption
                                                    | QRegularExpression::DotMatchesEverythingOption);
        const auto tm = titleRe.match(html);
        if (tm.hasMatch())
            m_title = QTextDocumentFragment::fromHtml(tm.captured(1)).toPlainText().simplified();

        static const QRegularExpression linkRe(QStringLiteral("<link\\b[^>]*>"),
                                               QRegularExpression::CaseInsensitiveOption);
        auto it = linkRe.globalMatch(html);
        while (it.hasNext()) {
            const QString tag = it.next().captured(0);
            const QString rel = attr(tag, QStringLiteral("rel")).toLower();
            if (!rel.contains(QLatin1String("icon")) || rel.contains(QLatin1String("mask-icon")))
                continue;
            const QString href = attr(tag, QStringLiteral("href"));
            if (!href.isEmpty())
                candidates << base.resolved(QUrl(href));
        }

        static const QRegularExpression metaRe(QStringLiteral("<meta\\b[^>]*>"),
                                               QRegularExpression::CaseInsensitiveOption);
        it = metaRe.globalMatch(html);
        while (it.hasNext()) {
            const QString tag = it.next().captured(0);
            const QString n = attr(tag, QStringLiteral("name")).toLower();
            if (n == QLatin1String("msapplication-tileimage")) {
                const QString c = attr(tag, QStringLiteral("content"));
                if (!c.isEmpty())
                    candidates << base.resolved(QUrl(c));
            }
        }
    }

    const QUrl root = base.isValid() && !base.host().isEmpty() ? base : reply->request().url();
    candidates << root.resolved(QUrl(QStringLiteral("/apple-touch-icon.png")))
               << root.resolved(QUrl(QStringLiteral("/favicon.ico")));
    if (!root.host().isEmpty()) {
        candidates << QUrl(QStringLiteral("https://icons.duckduckgo.com/ip3/%1.ico").arg(root.host()));
    }

    for (const QUrl &u : candidates)
        requestIcon(u);

    --m_pending;
    if (m_pending <= 0)
        done();
}

void FaviconFetcher::requestIcon(const QUrl &url)
{
    if (!url.isValid() || m_requested.contains(url.toString()))
        return;
    if (url.scheme() == QLatin1String("data")) {
        // data:image/png;base64,....
        const QString s = url.toString();
        const int comma = s.indexOf(QLatin1Char(','));
        if (comma > 0 && s.left(comma).contains(QLatin1String(";base64"))) {
            const QImage img = decodeImage(QByteArray::fromBase64(s.mid(comma + 1).toLatin1()));
            if (!img.isNull())
                m_icons << img;
        }
        return;
    }
    if (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))
        return;
    m_requested.insert(url.toString());
    QNetworkReply *r = get(url);
    connect(r, &QNetworkReply::finished, this, [this, r] { onIconReply(r); });
}

void FaviconFetcher::onIconReply(QNetworkReply *reply)
{
    m_replies.removeOne(reply);
    reply->deleteLater();
    if (reply->error() == QNetworkReply::NoError) {
        const QByteArray data = reply->read(4 * 1024 * 1024);
        const QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
        if (!m_hashes.contains(hash)) {
            m_hashes.insert(hash);
            const QImage img = decodeImage(data);
            if (!img.isNull() && img.width() >= 16 && img.height() >= 16)
                m_icons << img;
        }
    }
    --m_pending;
    if (m_pending <= 0)
        done();
}

void FaviconFetcher::done()
{
    m_pending = 0;
    std::stable_sort(m_icons.begin(), m_icons.end(), [](const QImage &a, const QImage &b) {
        return a.width() * a.height() > b.width() * b.height();
    });
    emit finished(m_icons, m_title);
}
