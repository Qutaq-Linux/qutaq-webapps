#pragma once

#include <QImage>
#include <QList>
#include <QObject>
#include <QSet>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

// Загружает страницу, ищет <link rel="icon">, apple-touch-icon и т.п.,
// скачивает все кандидаты и возвращает найденные картинки (крупные — первыми).
class FaviconFetcher : public QObject
{
    Q_OBJECT
public:
    explicit FaviconFetcher(QObject *parent = nullptr);

    void fetch(const QUrl &pageUrl);
    void abort();
    bool isRunning() const { return m_pending > 0; }

signals:
    void finished(const QList<QImage> &icons, const QString &pageTitle);

private:
    void onPageReply(QNetworkReply *reply);
    void requestIcon(const QUrl &url);
    void onIconReply(QNetworkReply *reply);
    void done();
    QNetworkReply *get(const QUrl &url);

    QNetworkAccessManager *m_nam;
    QList<QNetworkReply *> m_replies;
    QList<QImage> m_icons;
    QSet<QString> m_requested;
    QSet<QByteArray> m_hashes;
    QString m_title;
    int m_pending = 0;
};
