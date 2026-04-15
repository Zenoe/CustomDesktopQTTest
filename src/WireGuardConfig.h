#pragma once
// WireGuardConfig.h  –  Parse / serialise WireGuard .conf files
// Supports multi-peer configs as produced by wg-quick / official client.

#include <QString>
#include <QStringList>
#include <QList>
#include <QHostAddress>

struct WgPeer {
    QString  publicKey;
    QString  preSharedKey;          // optional
    QString  endpoint;              // host:port
    QStringList allowedIPs;
    int      persistentKeepalive = 0;
    QString  name;                  // GUI label (stored as comment # Name = ...)
};

struct WgInterface {
    QString  privateKey;
    QString  publicKey;             // derived, not stored in conf
    QStringList addresses;          // CIDR list
    quint16  listenPort = 0;
    QStringList dns;
    QString  mtu;
    // Pre/PostUp/Down hooks (informational; we don't execute them)
    QStringList preUp, postUp, preDown, postDown;
    QString  table;
};

struct WgConfig {
    QString      name;              // tunnel name (filename stem)
    QString      filePath;          // absolute path to .conf on disk
    WgInterface  iface;
    QList<WgPeer> peers;

    // Serialise to WireGuard .conf format
    QString toConfString() const;

    // Parse from .conf text; returns false + fills errorMsg on failure
    bool fromConfString(const QString& text, QString& errorMsg);

    // Convenience
    static WgConfig fromFile(const QString& path, QString& errorMsg);
    bool saveToFile(const QString& path, QString& errorMsg) const;
    bool saveToFile(QString& errorMsg) const { return saveToFile(filePath, errorMsg); }

    bool isValid() const;
};
