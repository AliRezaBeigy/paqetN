#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QMessageBox>
#include <QIcon>
#include "src/PaqetController.h"
#include "src/SingleInstanceGuard.h"

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Floor);
#endif

    QApplication app(argc, argv);

    app.setOrganizationName(QStringLiteral("paqetN"));
    app.setApplicationName(QStringLiteral("paqetN"));
    app.setApplicationDisplayName(QStringLiteral("paqetN"));
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/assets/icons/app_icon.png")));

    SingleInstanceGuard singleInstance;
    if (!singleInstance.tryLock()) {
        QMessageBox::warning(nullptr, QApplication::applicationDisplayName(),
            QObject::tr("paqetN is already running."));
        return 0;
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

    return app.exec();
}
