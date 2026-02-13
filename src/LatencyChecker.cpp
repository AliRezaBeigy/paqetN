#include "LatencyChecker.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkProxy>
#include <QUrl>

LatencyChecker::LatencyChecker(QObject *parent) : QObject(parent) {
    m_nam = new QNetworkAccessManager(this);
}

LatencyChecker::~LatencyChecker() {
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}

void LatencyChecker::check(int socksPort, const QString &url) {
    if (m_reply) {
        m_reply->disconnect(this);  // avoid onFinished() running for this reply after we replace m_reply
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    QString u = url.trimmed();
    if (u.isEmpty()) u = QStringLiteral("https://www.gstatic.com/generate_204");
    else if (!u.startsWith(QLatin1String("http://")) && !u.startsWith(QLatin1String("https://")))
        u = QStringLiteral("https://") + u;

    m_nam->setProxy(QNetworkProxy(QNetworkProxy::Socks5Proxy, QStringLiteral("127.0.0.1"), socksPort));
    QNetworkRequest req{QUrl(u)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(4000);
    m_timer.start();
    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::finished, this, &LatencyChecker::onFinished);
    emit started();
}

void LatencyChecker::onFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply || reply != m_reply) return;  // obsolete reply (we started a new check); only handle current
    const int ms = reply->error() == QNetworkReply::NoError
        ? static_cast<int>(m_timer.elapsed())
        : -1;
    m_reply = nullptr;  // clear before deleteLater so no re-entrancy uses stale m_reply
    reply->deleteLater();
    m_nam->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    emit result(ms);
    emit finished();
}
