#pragma once

#include "PaqetConfig.h"
#include <QObject>
#include <QString>
#include <QList>

class ConfigRepository : public QObject
{
    Q_OBJECT
public:
    explicit ConfigRepository(QObject *parent = nullptr);

    QList<PaqetConfig> configs() const;
    QString lastSelectedId() const;
    void setLastSelectedId(const QString &id);

    QString add(const PaqetConfig &config);
    void update(const PaqetConfig &config);
    void remove(const QString &id);
    void renameGroup(const QString &oldName, const QString &newName);

    PaqetConfig getById(const QString &id) const;

signals:
    void configsChanged();

private:
    QString configFilePath() const;
    bool load(QList<PaqetConfig> *out) const;
    bool save(const QList<PaqetConfig> &list);

    QString m_lastSelectedId;
};
