#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <optional>

struct PaqetConfig {
    QString id;
    QString name;
    QString group;
    QString serverAddr;
    QString networkInterface;
    QString ipv4Addr;
    QString routerMac;
    QString guid;  // Network interface GUID (Windows only)
    QString kcpKey;
    QString kcpBlock = QStringLiteral("aes");
    QString socksListen = QStringLiteral("127.0.0.1:1284");
    QStringList localFlag = { QStringLiteral("PA") };
    QStringList remoteFlag = { QStringLiteral("PA") };
    int conn = 1;
    QString kcpMode = QStringLiteral("fast");
    int mtu = 1350;
    int kcpNodelay = -1;
    int kcpInterval = -1;
    int kcpResend = -1;
    int kcpNocongestion = -1;
    bool kcpWdelay = false;
    bool kcpAcknodelay = true;

    static const QStringList &kcpBlockOptions();
    static const QStringList &kcpModeOptions();
    static constexpr const char *defaultTcpFlags = "PA";

    int socksPort() const;
    PaqetConfig withDefaults() const;
    QString toYaml(const QString &logLevel = QStringLiteral("debug")) const;
    QString toPaqetUri() const;
    QVariantMap toVariantMap() const;
    static PaqetConfig fromVariantMap(const QVariantMap &m);
    static std::optional<PaqetConfig> parseFromImport(const QString &text);
};
