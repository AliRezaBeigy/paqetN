#pragma once

#include "PaqetConfig.h"
#include <QAbstractListModel>
#include <QList>
#include <QVariantList>

class ConfigListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole,
        NameRole,
        ServerAddrRole,
        SocksListenRole,
        ConfigRole,
        GroupRole,
        KcpBlockRole,
        KcpModeRole
    };

    explicit ConfigListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setConfigs(const QList<PaqetConfig> &configs);
    PaqetConfig configAt(int row) const;
    Q_INVOKABLE QString configIdAt(int row) const;
    Q_INVOKABLE int indexOfId(const QString &id) const;
    Q_INVOKABLE QVariantList distinctGroups() const;

private:
    QList<PaqetConfig> m_configs;
};
