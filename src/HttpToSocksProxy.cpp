#include "HttpToSocksProxy.h"
#include "LogBuffer.h"
#include <QHostAddress>
#include <QRegularExpression>
#include <QUrl>
#include <QMetaObject>

// SOCKS5 constants
static constexpr quint8 SOCKS5_VERSION = 0x05;
static constexpr quint8 SOCKS5_AUTH_NONE = 0x00;
static constexpr quint8 SOCKS5_CMD_CONNECT = 0x01;
static constexpr quint8 SOCKS5_ATYP_DOMAIN = 0x03;
static constexpr quint8 SOCKS5_ATYP_IPV4 = 0x01;

class ProxyServerRunner;

/**
 * @brief Handles a single client connection (used from worker thread)
 */
class HttpToSocksProxy::ClientConnection : public QObject
{
    Q_OBJECT
public:
    using LogFunc = std::function<void(const QString &)>;

    ClientConnection(QTcpSocket *clientSocket, const QString &socksHost, quint16 socksPort,
                     LogFunc logFunc, QObject *parent)
        : QObject(parent)
        , m_client(clientSocket)
        , m_socksHost(socksHost)
        , m_socksPort(socksPort)
        , m_logFunc(std::move(logFunc))
    {
        m_client->setParent(this);
        m_socks = new QTcpSocket(this);

        connect(m_client, &QTcpSocket::readyRead, this, &ClientConnection::onClientReadyRead);
        connect(m_client, &QTcpSocket::disconnected, this, &ClientConnection::onClientDisconnected);
        connect(m_socks, &QTcpSocket::connected, this, &ClientConnection::onSocksConnected);
        connect(m_socks, &QTcpSocket::readyRead, this, &ClientConnection::onSocksReadyRead);
        connect(m_socks, &QTcpSocket::disconnected, this, &ClientConnection::onSocksDisconnected);
        connect(m_socks, &QTcpSocket::errorOccurred, this, &ClientConnection::onSocksError);
    }

    ~ClientConnection() override {
        if (m_client) m_client->close();
        if (m_socks) m_socks->close();
    }

signals:
    void finished();

private slots:
    void onClientReadyRead() {
        if (m_tunnelEstablished) {
            // Tunnel mode - just forward data
            QByteArray data = m_client->readAll();
            if (m_socks->state() == QAbstractSocket::ConnectedState) {
                m_socks->write(data);
            }
            return;
        }

        // Read HTTP request
        m_requestBuffer.append(m_client->readAll());

        // Check if we have complete headers
        int headerEnd = m_requestBuffer.indexOf("\r\n\r\n");
        if (headerEnd == -1) {
            return; // Wait for more data
        }

        // Parse the request line
        int firstLineEnd = m_requestBuffer.indexOf("\r\n");
        QString requestLine = QString::fromUtf8(m_requestBuffer.left(firstLineEnd));
        
        QStringList parts = requestLine.split(' ');
        if (parts.size() < 3) {
            sendError(400, "Bad Request");
            return;
        }

        m_method = parts[0].toUpper();
        m_requestUrl = parts[1];
        m_httpVersion = parts[2];

        // Parse headers
        QString headersStr = QString::fromUtf8(m_requestBuffer.mid(firstLineEnd + 2, headerEnd - firstLineEnd - 2));
        QStringList headerLines = headersStr.split("\r\n");
        for (const QString &line : headerLines) {
            int colonPos = line.indexOf(':');
            if (colonPos > 0) {
                QString key = line.left(colonPos).trimmed().toLower();
                QString value = line.mid(colonPos + 1).trimmed();
                m_headers.insert(key, value);
            }
        }

        // Store body if present
        m_requestBody = m_requestBuffer.mid(headerEnd + 4);

        // Extract target host and port
        if (m_method == QLatin1String("CONNECT")) {
            // CONNECT host:port HTTP/1.1
            QStringList hostPort = m_requestUrl.split(':');
            m_targetHost = hostPort[0];
            m_targetPort = hostPort.size() > 1 ? hostPort[1].toUShort() : 443;
            m_isConnect = true;
        } else {
            // Regular HTTP request - extract from URL or Host header
            if (m_requestUrl.startsWith(QLatin1String("http://"))) {
                QUrl url(m_requestUrl);
                m_targetHost = url.host();
                m_targetPort = url.port(80);
                // Rewrite URL to relative path for the request we send to the target
                m_requestUrl = url.path();
                if (m_requestUrl.isEmpty()) m_requestUrl = QStringLiteral("/");
                if (!url.query().isEmpty()) {
                    m_requestUrl += QStringLiteral("?") + url.query();
                }
            } else {
                QString host = m_headers.value(QStringLiteral("host"));
                QStringList hostPort = host.split(':');
                m_targetHost = hostPort[0];
                m_targetPort = hostPort.size() > 1 ? hostPort[1].toUShort() : 80;
            }
            m_isConnect = false;
        }

        if (m_targetHost.isEmpty()) {
            sendError(400, "Bad Request - No host");
            return;
        }

        log(QStringLiteral("[HTTP2SOCKS] %1 %2:%3").arg(m_method, m_targetHost).arg(m_targetPort));

        // Connect to SOCKS5 proxy
        m_state = State::ConnectingToSocks;
        m_socks->connectToHost(m_socksHost, m_socksPort);
    }

    void onSocksConnected() {
        // Send SOCKS5 greeting: version, num methods, methods
        QByteArray greeting;
        greeting.append(static_cast<char>(SOCKS5_VERSION));
        greeting.append(static_cast<char>(1)); // 1 auth method
        greeting.append(static_cast<char>(SOCKS5_AUTH_NONE)); // No auth
        m_socks->write(greeting);
        m_state = State::SocksGreeting;
    }

    void onSocksReadyRead() {
        m_socksBuffer.append(m_socks->readAll());

        switch (m_state) {
        case State::SocksGreeting:
            handleSocksGreeting();
            break;
        case State::SocksConnectRequest:
            handleSocksConnectResponse();
            break;
        case State::Tunneling:
            // Forward data to client
            if (m_client->state() == QAbstractSocket::ConnectedState) {
                m_client->write(m_socksBuffer);
            }
            m_socksBuffer.clear();
            break;
        default:
            break;
        }
    }

    void onClientDisconnected() {
        if (m_socks) m_socks->close();
        emit finished();
    }

    void onSocksDisconnected() {
        if (m_client) m_client->close();
        emit finished();
    }

    void onSocksError(QAbstractSocket::SocketError) {
        log(QStringLiteral("[HTTP2SOCKS] SOCKS error: %1").arg(m_socks->errorString()));
        sendError(502, "Bad Gateway - SOCKS connection failed");
    }

private:
    enum class State {
        WaitingForRequest,
        ConnectingToSocks,
        SocksGreeting,
        SocksConnectRequest,
        Tunneling
    };

    void handleSocksGreeting() {
        if (m_socksBuffer.size() < 2) return;

        quint8 version = static_cast<quint8>(m_socksBuffer[0]);
        quint8 method = static_cast<quint8>(m_socksBuffer[1]);
        m_socksBuffer.remove(0, 2);

        if (version != SOCKS5_VERSION || method != SOCKS5_AUTH_NONE) {
            sendError(502, "Bad Gateway - SOCKS auth failed");
            return;
        }

        // Send SOCKS5 connect request
        QByteArray connectReq;
        connectReq.append(static_cast<char>(SOCKS5_VERSION));
        connectReq.append(static_cast<char>(SOCKS5_CMD_CONNECT));
        connectReq.append(static_cast<char>(0x00)); // Reserved

        // Use domain name type
        QByteArray hostBytes = m_targetHost.toUtf8();
        connectReq.append(static_cast<char>(SOCKS5_ATYP_DOMAIN));
        connectReq.append(static_cast<char>(hostBytes.size()));
        connectReq.append(hostBytes);

        // Port in network byte order
        connectReq.append(static_cast<char>((m_targetPort >> 8) & 0xFF));
        connectReq.append(static_cast<char>(m_targetPort & 0xFF));

        m_socks->write(connectReq);
        m_state = State::SocksConnectRequest;
    }

    void handleSocksConnectResponse() {
        // Minimum response: version(1) + reply(1) + rsv(1) + atyp(1) + addr(varies) + port(2)
        if (m_socksBuffer.size() < 4) return;

        quint8 version = static_cast<quint8>(m_socksBuffer[0]);
        quint8 reply = static_cast<quint8>(m_socksBuffer[1]);
        quint8 atyp = static_cast<quint8>(m_socksBuffer[3]);

        // Calculate full response size
        int addrLen = 0;
        switch (atyp) {
        case SOCKS5_ATYP_IPV4:
            addrLen = 4;
            break;
        case SOCKS5_ATYP_DOMAIN:
            if (m_socksBuffer.size() < 5) return;
            addrLen = 1 + static_cast<quint8>(m_socksBuffer[4]);
            break;
        case 0x04: // IPv6
            addrLen = 16;
            break;
        default:
            sendError(502, "Bad Gateway - Unknown SOCKS address type");
            return;
        }

        int totalLen = 4 + addrLen + 2;
        if (m_socksBuffer.size() < totalLen) return;

        m_socksBuffer.remove(0, totalLen);

        if (version != SOCKS5_VERSION || reply != 0x00) {
            sendError(502, QStringLiteral("Bad Gateway - SOCKS connect failed (code %1)").arg(reply));
            return;
        }

        // SOCKS5 connection established
        m_tunnelEstablished = true;
        m_state = State::Tunneling;

        if (m_isConnect) {
            // Send 200 Connection Established for CONNECT
            QString response = QStringLiteral("HTTP/1.1 200 Connection Established\r\n\r\n");
            m_client->write(response.toUtf8());
        } else {
            // Forward the original HTTP request
            QByteArray request;
            request.append(m_method.toUtf8());
            request.append(' ');
            request.append(m_requestUrl.toUtf8());
            request.append(' ');
            request.append(m_httpVersion.toUtf8());
            request.append("\r\n");

            // Add headers
            for (auto it = m_headers.begin(); it != m_headers.end(); ++it) {
                // Skip proxy-specific headers
                if (it.key().startsWith(QLatin1String("proxy-"))) continue;
                request.append(it.key().toUtf8());
                request.append(": ");
                request.append(it.value().toUtf8());
                request.append("\r\n");
            }
            request.append("\r\n");
            request.append(m_requestBody);

            m_socks->write(request);
        }

        // Process any remaining data in the SOCKS buffer
        if (!m_socksBuffer.isEmpty() && m_client->state() == QAbstractSocket::ConnectedState) {
            m_client->write(m_socksBuffer);
            m_socksBuffer.clear();
        }
    }

    void sendError(int code, const QString &message) {
        QString response = QStringLiteral("HTTP/1.1 %1 %2\r\n"
                                          "Content-Type: text/plain\r\n"
                                          "Connection: close\r\n"
                                          "\r\n"
                                          "%2\r\n").arg(code).arg(message);
        m_client->write(response.toUtf8());
        m_client->disconnectFromHost();
    }

    void log(const QString &msg) {
        if (m_logFunc) m_logFunc(msg);
    }

    QTcpSocket *m_client = nullptr;
    QTcpSocket *m_socks = nullptr;
    QString m_socksHost;
    quint16 m_socksPort = 0;
    LogFunc m_logFunc;

    QByteArray m_requestBuffer;
    QByteArray m_socksBuffer;
    QByteArray m_requestBody;
    QString m_method;
    QString m_requestUrl;
    QString m_httpVersion;
    QMap<QString, QString> m_headers;
    QString m_targetHost;
    quint16 m_targetPort = 0;
    bool m_isConnect = false;
    bool m_tunnelEstablished = false;
    State m_state = State::WaitingForRequest;
};

/**
 * @brief Runs in worker thread: owns QTcpServer and all connection I/O
 * so that large proxy traffic does not block the GUI thread.
 */
class ProxyServerRunner : public QObject
{
    Q_OBJECT
public:
    explicit ProxyServerRunner(QObject *parent = nullptr) : QObject(parent) {}

public slots:
    bool startListen(quint16 httpPort, const QString &socksHost, quint16 socksPort) {
        m_socksHost = socksHost;
        m_socksPort = socksPort;
        m_httpPort = httpPort;

        if (!m_server) {
            m_server = new QTcpServer(this);
            connect(m_server, &QTcpServer::newConnection, this, &ProxyServerRunner::onNewConnection);
        }
        if (m_server->isListening())
            m_server->close();
        for (HttpToSocksProxy::ClientConnection *conn : m_connections) {
            conn->deleteLater();
        }
        m_connections.clear();

        if (!m_server->listen(QHostAddress::LocalHost, httpPort))
            return false;
        return true;
    }

    void stopListen() {
        if (m_server && m_server->isListening())
            m_server->close();
        for (HttpToSocksProxy::ClientConnection *conn : m_connections) {
            conn->deleteLater();
        }
        m_connections.clear();
    }

signals:
    void logRequested(const QString &message);

private slots:
    void onNewConnection() {
        while (m_server->hasPendingConnections()) {
            QTcpSocket *clientSocket = m_server->nextPendingConnection();
            auto logFunc = [this](const QString &msg) { emit logRequested(msg); };
            auto *conn = new HttpToSocksProxy::ClientConnection(
                clientSocket, m_socksHost, m_socksPort, logFunc, this);
            m_connections.append(conn);

            connect(conn, &HttpToSocksProxy::ClientConnection::finished, this, [this, conn]() {
                m_connections.removeOne(conn);
                conn->deleteLater();
            });
        }
    }

private:
    QTcpServer *m_server = nullptr;
    QString m_socksHost;
    quint16 m_socksPort = 0;
    quint16 m_httpPort = 0;
    QList<HttpToSocksProxy::ClientConnection*> m_connections;
};

// Include the moc file for the nested class and ProxyServerRunner
#include "HttpToSocksProxy.moc"

HttpToSocksProxy::HttpToSocksProxy(QObject *parent)
    : QObject(parent)
{
}

HttpToSocksProxy::~HttpToSocksProxy() {
    stop();
    if (m_thread) {
        m_thread->quit();
        if (!m_thread->wait(3000))
            m_thread->terminate();
        delete m_runner;
        m_runner = nullptr;
        m_thread = nullptr;
    }
}

bool HttpToSocksProxy::start(quint16 httpPort, const QString &socksHost, quint16 socksPort) {
    if (m_running)
        stop();

    m_socksHost = socksHost;
    m_socksPort = socksPort;
    m_httpPort = httpPort;

    if (!m_thread) {
        m_thread = new QThread(this);
        m_runner = new ProxyServerRunner();
        m_runner->moveToThread(m_thread);
        connect(static_cast<ProxyServerRunner *>(m_runner), &ProxyServerRunner::logRequested, this, &HttpToSocksProxy::onLogFromWorker, Qt::QueuedConnection);
        m_thread->start();
    }

    bool ok = false;
    const bool invoked = QMetaObject::invokeMethod(m_runner, "startListen",
        Qt::BlockingQueuedConnection,
        Q_RETURN_ARG(bool, ok),
        Q_ARG(quint16, httpPort),
        Q_ARG(QString, socksHost),
        Q_ARG(quint16, socksPort));
    if (!invoked || !ok) {
        if (invoked)
            log(QStringLiteral("[HTTP2SOCKS] Failed to start on port %1").arg(httpPort));
        emit error(tr("HTTP proxy failed to start"));
        return false;
    }

    m_running = true;
    log(QStringLiteral("[HTTP2SOCKS] Started HTTP proxy on 127.0.0.1:%1, forwarding to SOCKS5 %2:%3")
        .arg(httpPort).arg(socksHost).arg(socksPort));
    emit started();
    return true;
}

void HttpToSocksProxy::stop() {
    if (!m_running || !m_runner)
        return;
    QMetaObject::invokeMethod(m_runner, "stopListen", Qt::BlockingQueuedConnection);
    m_running = false;
    log(QStringLiteral("[HTTP2SOCKS] Stopped"));
    emit stopped();
}

void HttpToSocksProxy::onLogFromWorker(const QString &message) {
    if (m_logBuffer)
        m_logBuffer->append(message);
}

void HttpToSocksProxy::log(const QString &message) {
    if (m_logBuffer)
        m_logBuffer->append(message);
}
