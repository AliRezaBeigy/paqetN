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
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}

void LatencyChecker::check(int socksPort, const QString &url) {
    if (m_reply) {
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
    m_timer.start();
    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::finished, this, &LatencyChecker::onFinished);
    emit started();
}

void LatencyChecker::onFinished() {
    if (!m_reply) return;
    const int ms = m_reply->error() == QNetworkReply::NoError
        ? static_cast<int>(m_timer.elapsed())
        : -1;
    m_reply->deleteLater();
    m_reply = nullptr;
    m_nam->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    emit result(ms);
    emit finished();
}
