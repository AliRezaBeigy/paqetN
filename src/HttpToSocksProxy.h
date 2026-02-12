#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <QByteArray>

class LogBuffer;

/**
 * @brief HTTP-to-SOCKS5 proxy server
 * 
 * Accepts HTTP and HTTPS (via CONNECT) requests on a local port
 * and forwards them through a SOCKS5 proxy.
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
    bool isRunning() const;

    /**
     * @brief Get the HTTP port the proxy is listening on
     */
    quint16 httpPort() const { return m_httpPort; }

signals:
    void started();
    void stopped();
    void error(const QString &message);

private slots:
    void onNewConnection();

private:
    class ClientConnection;
    
    void log(const QString &message);

    QTcpServer *m_server = nullptr;
    LogBuffer *m_logBuffer = nullptr;
    QString m_socksHost;
    quint16 m_socksPort = 0;
    quint16 m_httpPort = 0;
    QList<ClientConnection*> m_connections;
};
