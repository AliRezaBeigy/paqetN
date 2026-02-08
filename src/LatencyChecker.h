#pragma once

#include <QObject>
#include <QElapsedTimer>

class QNetworkAccessManager;
class QNetworkReply;

class LatencyChecker : public QObject
{
    Q_OBJECT
public:
    explicit LatencyChecker(QObject *parent = nullptr);
    ~LatencyChecker() override;

    void check(int socksPort, const QString &url);

signals:
    void result(int ms);
    void started();
    void finished();

private:
    void onFinished();

    QNetworkAccessManager *m_nam = nullptr;
    QNetworkReply *m_reply = nullptr;
    QElapsedTimer m_timer;
};
