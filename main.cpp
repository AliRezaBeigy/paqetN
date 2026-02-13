#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QThread>
#include <QLocalServer>
#include <QLocalSocket>
#include <QWindow>
#include <QtQml/qqmlextensionplugin.h>
#include "src/PaqetController.h"
#include "src/SingleInstanceGuard.h"
#include "src/CrashHandler.h"

namespace {
const char kSingleInstanceServerName[] = "paqetN_single_instance";
}

#if defined(QT_STATIC) || !defined(QT_SHARED)
Q_IMPORT_QML_PLUGIN(FluentUIPlugin)
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Floor);
#endif

    QApplication app(argc, argv);

    // Install crash handler so on SIGSEGV/unhandled exception we write a report file
    // that users can share to help debug (stack trace, exception address, etc.).
    CrashHandler::install(app.applicationDirPath());
    CrashHandler::setExecutablePath(app.applicationFilePath());

    app.setOrganizationName(QStringLiteral("paqetN"));
    app.setApplicationName(QStringLiteral("paqetN"));
    app.setApplicationDisplayName(QStringLiteral("paqetN"));
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/assets/icons/app_icon.png")));

    const bool elevatedRestart = QCoreApplication::arguments().contains(
        QLatin1String(PaqetController::kElevatedRestartArg));

    SingleInstanceGuard singleInstance;
    if (!singleInstance.tryLock()) {
        if (elevatedRestart) {
            // Give the previous (non-elevated) process time to quit and release the lock
            const int retryMs = 4000;
            const int intervalMs = 200;
            for (int waited = 0; waited < retryMs; waited += intervalMs) {
                QThread::msleep(intervalMs);
                if (singleInstance.tryLock())
                    break;
                if (waited + intervalMs >= retryMs) {
                    // Tell existing instance to come to front, then exit
                    QLocalSocket socket;
                    socket.connectToServer(QLatin1String(kSingleInstanceServerName));
                    if (socket.waitForConnected(500))
                        socket.disconnectFromServer();
                    return 0;
                }
            }
        } else {
            // Tell existing instance to come to front, then exit (no dialog)
            QLocalSocket socket;
            socket.connectToServer(QLatin1String(kSingleInstanceServerName));
            if (socket.waitForConnected(500))
                socket.disconnectFromServer();
            return 0;
        }
    }

    QQmlApplicationEngine engine;
    PaqetController controller;
    engine.rootContext()->setContextProperty(QStringLiteral("paqetController"), &controller);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("paqetN", "Main");

    // When another instance is started, it connects here; bring main window to front
    QLocalServer raiseServer;
    QObject::connect(&raiseServer, &QLocalServer::newConnection, &app, [&raiseServer, &engine]() {
        if (QLocalSocket *client = raiseServer.nextPendingConnection()) {
            client->disconnectFromServer();
            client->deleteLater();
            QObject *root = engine.rootObjects().value(0);
            if (QWindow *win = qobject_cast<QWindow *>(root)) {
                win->show();
                win->raise();
                win->requestActivate();
            }
        }
    });
    raiseServer.listen(QLatin1String(kSingleInstanceServerName));

    return app.exec();
}
