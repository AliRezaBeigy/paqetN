/**
 * @file test_http2socks.cpp
 * @brief Test for HttpToSocksProxy
 * 
 * This test requires a running SOCKS5 proxy on localhost:1080
 * You can use paqet or any other SOCKS5 proxy for testing.
 * 
 * Build with:
 *   cd build
 *   cmake .. -DBUILD_TESTS=ON
 *   cmake --build . --target test_http2socks
 * 
 * Run:
 *   ./test_http2socks
 */

#include <QCoreApplication>
#include <QTcpSocket>
#include <QTimer>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkProxy>
#include "../src/HttpToSocksProxy.h"
#include "../src/LogBuffer.h"

#include <cstdio>

// Force output flushing
#define LOG(msg) do { fprintf(stderr, "%s\n", qPrintable(msg)); fflush(stderr); } while(0)

class TestRunner : public QObject
{
    Q_OBJECT
public:
    TestRunner(QObject *parent = nullptr) : QObject(parent) {}

    void run() {
        LOG("=== HTTP to SOCKS5 Proxy Test ===");
        LOG("");

        // First check if SOCKS5 proxy is available
        LOG("Checking for SOCKS5 proxy on localhost:1080...");
        if (!checkSocksProxy()) {
            LOG("FAILED: No SOCKS5 proxy available on localhost:1080");
            LOG("Please start paqet or another SOCKS5 proxy first.");
            LOG("");
            LOG("You can skip the SOCKS requirement and just test the HTTP proxy starts:");
            testProxyStartsOnly();
            return;
        }
        LOG("SOCKS5 proxy found!");

        // Create and start HTTP proxy
        m_logBuffer = new LogBuffer(this);
        m_httpProxy = new HttpToSocksProxy(this);
        m_httpProxy->setLogBuffer(m_logBuffer);

        connect(m_logBuffer, &LogBuffer::logAppended, this, [this]() {
            QString text = m_logBuffer->fullText();
            QStringList lines = text.split('\n');
            if (!lines.isEmpty()) {
                LOG(QString("[LOG] %1").arg(lines.last()));
            }
        });

        if (!m_httpProxy->start(18080, "127.0.0.1", 1080)) {
            LOG("FAILED: Could not start HTTP proxy on port 18080");
            QCoreApplication::exit(1);
            return;
        }

        LOG("HTTP proxy started on port 18080");

        // Run tests
        QTimer::singleShot(500, this, &TestRunner::runTests);
    }

private slots:
    void runTests() {
        LOG("");
        LOG("--- Test 1: HTTP GET via HTTP proxy ---");
        testHttpGet();
    }

    void testHttpGet() {
        auto *nam = new QNetworkAccessManager(this);

        // Set our HTTP proxy
        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::HttpProxy);
        proxy.setHostName("127.0.0.1");
        proxy.setPort(18080);
        nam->setProxy(proxy);

        QNetworkRequest request(QUrl("http://httpbin.org/ip"));
        request.setHeader(QNetworkRequest::UserAgentHeader, "HttpToSocksTest/1.0");
        request.setTransferTimeout(10000);

        QNetworkReply *reply = nam->get(request);

        connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray data = reply->readAll();
                LOG("PASSED: HTTP GET succeeded");
                LOG(QString("Response: %1").arg(QString::fromUtf8(data.left(200))));
                m_httpTestPassed = true;
            } else {
                LOG(QString("FAILED: HTTP GET failed: %1").arg(reply->errorString()));
                m_httpTestPassed = false;
            }
            reply->deleteLater();
            nam->deleteLater();

            // Continue to HTTPS test
            QTimer::singleShot(500, this, &TestRunner::testHttpsConnect);
        });
    }

    void testHttpsConnect() {
        LOG("");
        LOG("--- Test 2: HTTPS CONNECT via HTTP proxy ---");

        auto *nam = new QNetworkAccessManager(this);

        // Set our HTTP proxy
        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::HttpProxy);
        proxy.setHostName("127.0.0.1");
        proxy.setPort(18080);
        nam->setProxy(proxy);

        QNetworkRequest request(QUrl("https://httpbin.org/ip"));
        request.setHeader(QNetworkRequest::UserAgentHeader, "HttpToSocksTest/1.0");
        request.setTransferTimeout(10000);

        QNetworkReply *reply = nam->get(request);

        connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray data = reply->readAll();
                LOG("PASSED: HTTPS CONNECT succeeded");
                LOG(QString("Response: %1").arg(QString::fromUtf8(data.left(200))));
                m_httpsTestPassed = true;
            } else {
                LOG(QString("FAILED: HTTPS CONNECT failed: %1").arg(reply->errorString()));
                LOG(QString("Error code: %1").arg(reply->error()));
                m_httpsTestPassed = false;
            }
            reply->deleteLater();
            nam->deleteLater();

            // Finish tests
            QTimer::singleShot(100, this, &TestRunner::finishTests);
        });
    }

    void finishTests() {
        if (m_httpProxy) m_httpProxy->stop();

        LOG("");
        LOG("=== Test Results ===");
        LOG(QString("HTTP GET test: %1").arg(m_httpTestPassed ? "PASSED" : "FAILED"));
        LOG(QString("HTTPS CONNECT test: %1").arg(m_httpsTestPassed ? "PASSED" : "FAILED"));

        int exitCode = (m_httpTestPassed && m_httpsTestPassed) ? 0 : 1;
        LOG("");
        LOG(QString("Overall: %1").arg(exitCode == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED"));

        QCoreApplication::exit(exitCode);
    }

private:
    void testProxyStartsOnly() {
        LOG("");
        LOG("--- Fallback Test: Just checking if HTTP proxy can start ---");
        
        m_logBuffer = new LogBuffer(this);
        m_httpProxy = new HttpToSocksProxy(this);
        m_httpProxy->setLogBuffer(m_logBuffer);

        // Try to start with a fake SOCKS proxy (won't work for actual requests)
        if (m_httpProxy->start(18080, "127.0.0.1", 1080)) {
            LOG("PASSED: HTTP proxy started successfully on port 18080");
            LOG("(Cannot test actual proxying without a SOCKS5 proxy)");
            m_httpProxy->stop();
            LOG("HTTP proxy stopped.");
            LOG("");
            LOG("=== Test Results ===");
            LOG("HTTP Proxy Start: PASSED");
            LOG("");
            LOG("To run full tests, start a SOCKS5 proxy on localhost:1080 first.");
            QCoreApplication::exit(0);
        } else {
            LOG("FAILED: Could not start HTTP proxy on port 18080");
            QCoreApplication::exit(1);
        }
    }

    bool checkSocksProxy() {
        QTcpSocket socket;
        socket.connectToHost("127.0.0.1", 1080);
        if (!socket.waitForConnected(2000)) {
            return false;
        }
        socket.close();
        return true;
    }

    LogBuffer *m_logBuffer = nullptr;
    HttpToSocksProxy *m_httpProxy = nullptr;
    bool m_httpTestPassed = false;
    bool m_httpsTestPassed = false;
};

int main(int argc, char *argv[])
{
    LOG("Starting test_http2socks...");
    
    QCoreApplication app(argc, argv);

    TestRunner runner;
    QTimer::singleShot(0, &runner, &TestRunner::run);

    return app.exec();
}

#include "test_http2socks.moc"
