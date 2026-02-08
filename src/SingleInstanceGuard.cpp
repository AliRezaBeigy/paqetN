#include "SingleInstanceGuard.h"
#include <QStandardPaths>
#include <QDir>

SingleInstanceGuard::SingleInstanceGuard(QObject *parent) : QObject(parent) {
    const QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QLatin1String("/paqetN_single_instance.lock");
    m_lockFile = new QLockFile(path);
    m_lockFile->setStaleLockTime(0);
}

SingleInstanceGuard::~SingleInstanceGuard() {
    release();
    delete m_lockFile;
    m_lockFile = nullptr;
}

bool SingleInstanceGuard::tryLock() {
    return m_lockFile && m_lockFile->tryLock();
}

void SingleInstanceGuard::release() {
    if (m_lockFile) m_lockFile->unlock();
}
