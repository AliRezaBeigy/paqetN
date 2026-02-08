#pragma once

#include <QObject>
#include <QString>
#include <QLockFile>

class SingleInstanceGuard : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstanceGuard(QObject *parent = nullptr);
    ~SingleInstanceGuard() override;
    bool tryLock();
    void release();

private:
    QLockFile *m_lockFile = nullptr;
};
