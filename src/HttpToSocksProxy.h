#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <QByteArray>
#include <QThread>
#include <functional>

class LogBuffer;
class ProxyServerRunner;

/**
 * @brief HTTP-to-SOCKS5 proxy server
 *
 * Accepts HTTP and HTTPS (via CONNECT) requests on a local port
 * and forwards them through a SOCKS5 proxy.
 * Runs the server and all connection I/O in a dedicated thread so that
 * large downloads (e.g. IDM) do not freeze the UI.
 */
class HttpToSocksProxy : public QObject
{
    Q_OBJECT
public:
    explicit HttpToSocksProxy(QObject *parent = nullptr);
    ~HttpToSocksProxy() override;

    void setLogBuffer(LogBuffer *logBuffer) { m_logBuffer = logBuffer; }

    /**
     * @brief Start the HTTP proxy server
     * @param httpPort Port to listen on for HTTP requests
     * @param socksHost SOCKS5 proxy host (e.g., "127.0.0.1")
     * @param socksPort SOCKS5 proxy port (e.g., 1080)
     * @return true if started successfully
     */
    bool start(quint16 httpPort, const QString &socksHost, quint16 socksPort);

    /**
     * @brief Stop the HTTP proxy server
     */
    void stop();

    /**
     * @brief Check if the proxy is running
     */
    bool isRunning() const { return m_running; }

    /**
     * @brief Get the HTTP port the proxy is listening on
     */
    quint16 httpPort() const { return m_httpPort; }

signals:
    void started();
    void stopped();
    void error(const QString &message);

private slots:
    void onLogFromWorker(const QString &message);

private:
    class ClientConnection;
    friend class ProxyServerRunner;  // defined in .cpp; needs ClientConnection access

    void log(const QString &message);

    LogBuffer *m_logBuffer = nullptr;
    QString m_socksHost;
    quint16 m_socksPort = 0;
    quint16 m_httpPort = 0;
    bool m_running = false;

    QThread *m_thread = nullptr;
    QObject *m_runner = nullptr;  // ProxyServerRunner, lives in m_thread
};
