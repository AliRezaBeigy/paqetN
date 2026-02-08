#include "ConfigListModel.h"

ConfigListModel::ConfigListModel(QObject *parent) : QAbstractListModel(parent) {}

int ConfigListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_configs.size();
}

QVariant ConfigListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_configs.size())
        return QVariant();
    const PaqetConfig &c = m_configs.at(index.row());
    switch (role) {
    case IdRole: return c.id;
    case NameRole: return c.name.isEmpty() ? c.serverAddr : c.name;
    case ServerAddrRole: return c.serverAddr;
    case SocksListenRole: return c.socksListen;
    case ConfigRole: return c.toVariantMap();
    case GroupRole: return c.group;
    case KcpBlockRole: return c.kcpBlock;
    case KcpModeRole: return c.kcpMode;
    default: return QVariant();
    }
}

QHash<int, QByteArray> ConfigListModel::roleNames() const {
    return {
        { IdRole, "configId" },
        { NameRole, "name" },
        { ServerAddrRole, "serverAddr" },
        { SocksListenRole, "socksListen" },
        { ConfigRole, "config" },
        { GroupRole, "group" },
        { KcpBlockRole, "kcpBlock" },
        { KcpModeRole, "kcpMode" }
    };
}

void ConfigListModel::setConfigs(const QList<PaqetConfig> &configs) {
    beginResetModel();
    m_configs = configs;
    endResetModel();
}

PaqetConfig ConfigListModel::configAt(int row) const {
    if (row < 0 || row >= m_configs.size()) return PaqetConfig();
    return m_configs.at(row);
}

QString ConfigListModel::configIdAt(int row) const {
    if (row < 0 || row >= m_configs.size()) return QString();
    return m_configs.at(row).id;
}

int ConfigListModel::indexOfId(const QString &id) const {
    for (int i = 0; i < m_configs.size(); ++i)
        if (m_configs.at(i).id == id) return i;
    return -1;
}

QVariantList ConfigListModel::distinctGroups() const {
    QMap<QString, int> counts;
    for (const PaqetConfig &c : m_configs) {
        QString g = c.group.isEmpty() ? QStringLiteral("Ungrouped") : c.group;
        counts[g]++;
    }
    QVariantList result;
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), it.key());
        entry.insert(QStringLiteral("count"), it.value());
        result.append(entry);
    }
    return result;
}
