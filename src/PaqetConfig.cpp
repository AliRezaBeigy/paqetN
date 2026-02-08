#include "PaqetConfig.h"
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtCore/qnumeric.h>

static const QStringList kcpBlockList = {
    QStringLiteral("aes"), QStringLiteral("aes-128"), QStringLiteral("aes-128-gcm"),
    QStringLiteral("aes-192"), QStringLiteral("salsa20"), QStringLiteral("blowfish"),
    QStringLiteral("twofish"), QStringLiteral("cast5"), QStringLiteral("3des"),
    QStringLiteral("tea"), QStringLiteral("xtea"), QStringLiteral("xor"),
    QStringLiteral("sm4"), QStringLiteral("none")
};

static const QStringList kcpModeList = {
    QStringLiteral("normal"), QStringLiteral("fast"), QStringLiteral("fast2"),
    QStringLiteral("fast3"), QStringLiteral("manual")
};

const QStringList &PaqetConfig::kcpBlockOptions() { return kcpBlockList; }
const QStringList &PaqetConfig::kcpModeOptions() { return kcpModeList; }

int PaqetConfig::socksPort() const {
    const int colon = socksListen.lastIndexOf(QLatin1Char(':'));
    if (colon < 0) return 1284;
    bool ok = false;
    const int p = socksListen.mid(colon + 1).toInt(&ok);
    return ok && p > 0 && p < 65536 ? p : 1284;
}

PaqetConfig PaqetConfig::withDefaults() const {
    PaqetConfig c = *this;
    if (!kcpBlockList.contains(c.kcpBlock)) c.kcpBlock = QStringLiteral("aes");
    if (!kcpModeList.contains(c.kcpMode)) c.kcpMode = QStringLiteral("fast");
    if (c.socksListen.isEmpty()) c.socksListen = QStringLiteral("127.0.0.1:1284");
    c.conn = qBound(1, c.conn, 256);
    c.mtu = (c.mtu < 50 || c.mtu > 1500) ? 1350 : c.mtu;
    const bool manual = (c.kcpMode == QStringLiteral("manual"));
    if (manual) {
        if (c.kcpNodelay < 0) c.kcpNodelay = 1;
        else c.kcpNodelay = qBound(0, c.kcpNodelay, 1);
        if (c.kcpInterval < 0) c.kcpInterval = 10;
        else c.kcpInterval = qBound(10, c.kcpInterval, 5000);
        if (c.kcpResend < 0) c.kcpResend = 2;
        else c.kcpResend = qBound(0, c.kcpResend, 2);
        if (c.kcpNocongestion < 0) c.kcpNocongestion = 1;
        else c.kcpNocongestion = qBound(0, c.kcpNocongestion, 1);
    } else {
        c.kcpNodelay = c.kcpInterval = c.kcpResend = c.kcpNocongestion = -1;
    }
    if (c.localFlag.isEmpty()) c.localFlag = { QStringLiteral("PA") };
    if (c.remoteFlag.isEmpty()) c.remoteFlag = { QStringLiteral("PA") };
    return c;
}

static QStringList variantToFlagList(const QVariant &v) {
    QStringList result;
    const int typeId = v.typeId();
    // Only use toList() for actual list types (not QString, which Qt 6 splits by character)
    if (typeId == QMetaType::QVariantList || typeId == QMetaType::QStringList) {
        for (const QVariant &x : v.toList()) {
            const QString s = x.toString().trimmed();
            if (!s.isEmpty()) result << s;
        }
        return result;
    }
    // For strings: split by comma
    const QString s = v.toString().trimmed();
    if (!s.isEmpty()) {
        for (const QString &part : s.split(QLatin1Char(','))) {
            const QString trimmed = part.trimmed();
            if (!trimmed.isEmpty()) result << trimmed;
        }
    }
    return result;
}

static QString tcpFlagYaml(const QStringList &list) {
    if (list.isEmpty()) return QStringLiteral("[ \"PA\" ]");
    QStringList out;
    for (const QString &s : list)
        out << QLatin1Char('"') + s.trimmed() + QLatin1Char('"');
    return QLatin1Char('[') + out.join(QLatin1String(", ")) + QLatin1Char(']');
}

QString PaqetConfig::toYaml(const QString &logLevel) const {
    const PaqetConfig c = withDefaults();
    const QString localYaml = tcpFlagYaml(c.localFlag);
    const QString remoteYaml = tcpFlagYaml(c.remoteFlag);
    QString manualParams;
    if (c.kcpMode == QLatin1String("manual")) {
        manualParams = QStringLiteral("\n    nodelay: %1\n    interval: %2\n    resend: %3\n    nocongestion: %4\n    wdelay: %5\n    acknodelay: %6")
            .arg(c.kcpNodelay).arg(c.kcpInterval).arg(c.kcpResend)
            .arg(c.kcpNocongestion).arg(c.kcpWdelay ? QLatin1String("true") : QLatin1String("false"))
            .arg(c.kcpAcknodelay ? QLatin1String("true") : QLatin1String("false"));
    }

    // Build network section with optional guid field
    // Ensure we have defaults if network info wasn't auto-detected
    QString networkInterface = c.networkInterface.isEmpty() ? QStringLiteral("lo") : c.networkInterface;
    QString ipv4Addr = c.ipv4Addr.isEmpty() ? QStringLiteral("127.0.0.1:0") : c.ipv4Addr;
    QString routerMac = c.routerMac.isEmpty() ? QStringLiteral("00:00:00:00:00:00") : c.routerMac;
    
    QString networkSection;
#ifdef Q_OS_WIN
    // On Windows, always include both interface (adapter name) and guid when guid is available
    if (!c.guid.isEmpty()) {
        // Both interface and guid are required on Windows with Npcap
        networkSection = QStringLiteral(
            "network:\n  interface: \"%1\"\n  guid: \"%2\"\n  ipv4:\n    addr: \"%3\"\n    router_mac: \"%4\"\n  tcp:\n    local_flag: %5\n    remote_flag: %6\n"
        ).arg(networkInterface.isEmpty() ? QStringLiteral("Ethernet") : networkInterface, 
              c.guid, ipv4Addr, routerMac, localYaml, remoteYaml);
    } else {
        // Fallback if guid is not set - use interface name only
        networkSection = QStringLiteral(
            "network:\n  interface: \"%1\"\n  ipv4:\n    addr: \"%2\"\n    router_mac: \"%3\"\n  tcp:\n    local_flag: %4\n    remote_flag: %5\n"
        ).arg(networkInterface.isEmpty() ? QStringLiteral("Ethernet") : networkInterface, 
              ipv4Addr, routerMac, localYaml, remoteYaml);
    }
#else
    networkSection = QStringLiteral(
        "network:\n  interface: \"%1\"\n  ipv4:\n    addr: \"%2\"\n    router_mac: \"%3\"\n  tcp:\n    local_flag: %4\n    remote_flag: %5\n"
    ).arg(networkInterface, ipv4Addr, routerMac, localYaml, remoteYaml);
#endif

    return QStringLiteral(
        "role: \"client\"\n"
        "log:\n  level: \"%1\"\n"
        "socks5:\n  - listen: \"%2\"\n    username: \"\"\n    password: \"\"\n"
        "%3"
        "server:\n  addr: \"%4\"\n"
        "transport:\n  protocol: \"kcp\"\n  conn: %5\n  kcp:\n    mode: \"%6\"\n    mtu: %7\n    rcvwnd: 512\n    sndwnd: 512\n    block: \"%8\"\n    key: \"%9\"%10\n"
    ).arg(logLevel, c.socksListen, networkSection, c.serverAddr).arg(c.conn)
     .arg(c.kcpMode).arg(c.mtu).arg(c.kcpBlock).arg(c.kcpKey).arg(manualParams);
}

QString PaqetConfig::toPaqetUri() const {
    const PaqetConfig c = withDefaults();
    QString base = QStringLiteral("paqet://") + c.serverAddr.trimmed();
    QList<QPair<QString, QString>> params;
    if (c.kcpBlock != QLatin1String("aes"))
        params.append({ QStringLiteral("enc"), c.kcpBlock });
    QString localStr = c.localFlag.join(QLatin1Char(','));
    if (localStr != QLatin1String("PA")) params.append({ QStringLiteral("local"), localStr });
    QString remoteStr = c.remoteFlag.join(QLatin1Char(','));
    if (remoteStr != QLatin1String("PA")) params.append({ QStringLiteral("remote"), remoteStr });
    params.append({ QStringLiteral("key"), c.kcpKey });
    if (c.conn != 1) params.append({ QStringLiteral("conn"), QString::number(c.conn) });
    if (c.kcpMode != QLatin1String("fast")) params.append({ QStringLiteral("mode"), c.kcpMode });
    if (c.mtu != 1350) params.append({ QStringLiteral("mtu"), QString::number(c.mtu) });
    if (c.kcpMode == QLatin1String("manual")) {
        if (c.kcpNodelay >= 0) params.append({ QStringLiteral("nodelay"), QString::number(c.kcpNodelay) });
        if (c.kcpInterval >= 0) params.append({ QStringLiteral("interval"), QString::number(c.kcpInterval) });
        if (c.kcpResend >= 0) params.append({ QStringLiteral("resend"), QString::number(c.kcpResend) });
        if (c.kcpNocongestion >= 0) params.append({ QStringLiteral("nocongestion"), QString::number(c.kcpNocongestion) });
        params.append({ QStringLiteral("wdelay"), c.kcpWdelay ? QLatin1String("true") : QLatin1String("false") });
        params.append({ QStringLiteral("acknodelay"), c.kcpAcknodelay ? QLatin1String("true") : QLatin1String("false") });
    }
    if (!params.isEmpty()) {
        QUrlQuery q;
        for (const auto &p : params) q.addQueryItem(p.first, p.second);
        base += QLatin1Char('?') + q.query(QUrl::FullyEncoded);
    }
    const QString nameVal = c.name.isEmpty() ? c.serverAddr : c.name;
    if (!nameVal.isEmpty())
        base += QLatin1Char('#') + QUrl::toPercentEncoding(nameVal);
    return base;
}

QVariantMap PaqetConfig::toVariantMap() const {
    QVariantMap m;
    m.insert(QStringLiteral("id"), id);
    m.insert(QStringLiteral("name"), name);
    m.insert(QStringLiteral("group"), group);
    m.insert(QStringLiteral("serverAddr"), serverAddr);
    m.insert(QStringLiteral("networkInterface"), networkInterface);
    m.insert(QStringLiteral("ipv4Addr"), ipv4Addr);
    m.insert(QStringLiteral("routerMac"), routerMac);
    m.insert(QStringLiteral("guid"), guid);
    m.insert(QStringLiteral("kcpKey"), kcpKey);
    m.insert(QStringLiteral("kcpBlock"), kcpBlock);
    m.insert(QStringLiteral("socksListen"), socksListen);
    QVariantList lfList, rfList;
    for (const QString &s : localFlag) lfList << s;
    for (const QString &s : remoteFlag) rfList << s;
    m.insert(QStringLiteral("localFlag"), lfList);
    m.insert(QStringLiteral("remoteFlag"), rfList);
    m.insert(QStringLiteral("conn"), conn);
    m.insert(QStringLiteral("kcpMode"), kcpMode);
    m.insert(QStringLiteral("mtu"), mtu);
    if (kcpNodelay >= 0) m.insert(QStringLiteral("kcpNodelay"), kcpNodelay);
    if (kcpInterval >= 0) m.insert(QStringLiteral("kcpInterval"), kcpInterval);
    if (kcpResend >= 0) m.insert(QStringLiteral("kcpResend"), kcpResend);
    if (kcpNocongestion >= 0) m.insert(QStringLiteral("kcpNocongestion"), kcpNocongestion);
    m.insert(QStringLiteral("kcpWdelay"), kcpWdelay);
    m.insert(QStringLiteral("kcpAcknodelay"), kcpAcknodelay);
    return m;
}

PaqetConfig PaqetConfig::fromVariantMap(const QVariantMap &m) {
    PaqetConfig c;
    c.id = m.value(QStringLiteral("id")).toString();
    c.name = m.value(QStringLiteral("name")).toString();
    c.group = m.value(QStringLiteral("group")).toString();
    c.serverAddr = m.value(QStringLiteral("serverAddr")).toString();
    c.networkInterface = m.value(QStringLiteral("networkInterface")).toString();
    c.ipv4Addr = m.value(QStringLiteral("ipv4Addr")).toString();
    c.routerMac = m.value(QStringLiteral("routerMac")).toString();
    c.guid = m.value(QStringLiteral("guid")).toString();
    c.kcpKey = m.value(QStringLiteral("kcpKey")).toString();
    c.kcpBlock = m.value(QStringLiteral("kcpBlock")).toString();
    c.socksListen = m.value(QStringLiteral("socksListen")).toString();
    // Preserve empty arrays - don't force default "PA" here
    // Empty arrays will be handled by withDefaults() only for new configs
    if (m.contains(QStringLiteral("localFlag"))) {
        c.localFlag = variantToFlagList(m.value(QStringLiteral("localFlag")));
        // Don't default empty arrays - preserve them to allow explicit empty flags
    }
    if (m.contains(QStringLiteral("remoteFlag"))) {
        c.remoteFlag = variantToFlagList(m.value(QStringLiteral("remoteFlag")));
        // Don't default empty arrays - preserve them to allow explicit empty flags
    }
    c.conn = m.value(QStringLiteral("conn")).toInt();
    c.kcpMode = m.value(QStringLiteral("kcpMode")).toString();
    c.mtu = m.value(QStringLiteral("mtu")).toInt();
    bool ok;
    int n = m.value(QStringLiteral("kcpNodelay")).toInt(&ok);
    c.kcpNodelay = ok ? n : -1;
    n = m.value(QStringLiteral("kcpInterval")).toInt(&ok);
    c.kcpInterval = ok ? n : -1;
    n = m.value(QStringLiteral("kcpResend")).toInt(&ok);
    c.kcpResend = ok ? n : -1;
    n = m.value(QStringLiteral("kcpNocongestion")).toInt(&ok);
    c.kcpNocongestion = ok ? n : -1;
    c.kcpWdelay = m.value(QStringLiteral("kcpWdelay")).toBool();
    c.kcpAcknodelay = m.value(QStringLiteral("kcpAcknodelay")).toBool();
    return c.withDefaults();
}

std::optional<PaqetConfig> PaqetConfig::parseFromImport(const QString &text) {
    const QString t = text.trimmed();
    if (t.isEmpty()) return std::nullopt;

    if (t.startsWith(QLatin1String("paqet://"))) {
        QString rest = t.mid(8);
        const int hashIdx = rest.indexOf(QLatin1Char('#'));
        QString beforeFragment = hashIdx < 0 ? rest : rest.left(hashIdx);
        QString fragment = hashIdx >= 0 ? rest.mid(hashIdx + 1) : QString();
        QString name = fragment.isEmpty() ? QString() : QUrl::fromPercentEncoding(fragment.toUtf8());
        const int qStart = beforeFragment.indexOf(QLatin1Char('?'));
        QString authority = qStart < 0 ? beforeFragment : beforeFragment.left(qStart);
        QString queryStr = qStart < 0 ? QString() : beforeFragment.mid(qStart + 1);

        QMap<QString, QString> params;
        for (const QString &part : queryStr.split(QLatin1Char('&'))) {
            int eq = part.indexOf(QLatin1Char('='));
            if (eq < 0) params.insert(part, QString());
            else params.insert(part.left(eq), QUrl::fromPercentEncoding(part.mid(eq + 1).toUtf8()));
        }
        auto get = [&params](const QString &key) -> QString {
            return params.value(key).trimmed();
        };

        PaqetConfig c;
        c.id = QString();
        c.name = name.isEmpty() ? authority : name;
        c.serverAddr = authority;
#ifdef Q_OS_WIN
        c.networkInterface = QString();  // Will be empty, user must set guid
        c.guid = QString();  // User must provide Windows GUID
#else
        c.networkInterface = QStringLiteral("lo");
        c.guid = QString();
#endif
        c.ipv4Addr = QStringLiteral("127.0.0.1:0");
        c.routerMac = QStringLiteral("00:00:00:00:00:00");
        c.kcpKey = get(QStringLiteral("key"));
        QString enc = get(QStringLiteral("enc"));
        c.kcpBlock = kcpBlockList.contains(enc) ? enc : QStringLiteral("aes");
        c.socksListen = QStringLiteral("127.0.0.1:1284");
        QString localStr = get(QStringLiteral("local"));
        if (!localStr.isEmpty()) {
            c.localFlag.clear();
            for (const QString &s : localStr.split(QLatin1Char(',')))
                if (!s.trimmed().isEmpty()) c.localFlag << s.trimmed();
        }
        if (c.localFlag.isEmpty()) c.localFlag = { QStringLiteral("PA") };
        QString remoteStr = get(QStringLiteral("remote"));
        if (!remoteStr.isEmpty()) {
            c.remoteFlag.clear();
            for (const QString &s : remoteStr.split(QLatin1Char(',')))
                if (!s.trimmed().isEmpty()) c.remoteFlag << s.trimmed();
        }
        if (c.remoteFlag.isEmpty()) c.remoteFlag = { QStringLiteral("PA") };
        bool ok;
        int connVal = get(QStringLiteral("conn")).toInt(&ok);
        c.conn = (ok && connVal >= 1 && connVal <= 256) ? connVal : 1;
        QString mode = get(QStringLiteral("mode"));
        c.kcpMode = kcpModeList.contains(mode) ? mode : QStringLiteral("fast");
        int mtuVal = get(QStringLiteral("mtu")).toInt(&ok);
        c.mtu = (ok && mtuVal >= 50 && mtuVal <= 1500) ? mtuVal : 1350;
        if (c.kcpMode == QLatin1String("manual")) {
            c.kcpNodelay = get(QStringLiteral("nodelay")).toInt(&ok);
            if (!ok) c.kcpNodelay = 1;
            else c.kcpNodelay = qBound(0, c.kcpNodelay, 1);
            c.kcpInterval = get(QStringLiteral("interval")).toInt(&ok);
            if (!ok) c.kcpInterval = 10;
            else c.kcpInterval = qBound(10, c.kcpInterval, 5000);
            c.kcpResend = get(QStringLiteral("resend")).toInt(&ok);
            if (!ok) c.kcpResend = 2;
            else c.kcpResend = qBound(0, c.kcpResend, 2);
            c.kcpNocongestion = get(QStringLiteral("nocongestion")).toInt(&ok);
            if (!ok) c.kcpNocongestion = 1;
            else c.kcpNocongestion = qBound(0, c.kcpNocongestion, 1);
            QString wd = get(QStringLiteral("wdelay"));
            c.kcpWdelay = (wd == QLatin1String("true") || wd == QLatin1String("1"));
            QString an = get(QStringLiteral("acknodelay"));
            c.kcpAcknodelay = (an != QLatin1String("false") && an != QLatin1String("0"));
        }
        return c.withDefaults();
    }

    // JSON line
    QString jsonLine;
    for (const QString &line : t.split(QLatin1Char('\n'))) {
        if (line.trimmed().startsWith(QLatin1Char('{'))) {
            jsonLine = line.trimmed();
            break;
        }
    }
    if (jsonLine.isEmpty()) jsonLine = t;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonLine.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return std::nullopt;
    QVariantMap m = doc.object().toVariantMap();
    return fromVariantMap(m);
}
