#pragma once
// WireGuardManager.h
// Controls the WireGuard tunnel Windows service via SCM.
// tunnel.dll's WireGuardTunnelService() runs as a service entry point;
// we install/start/stop it through the SCM API – exactly like the
// official wireguard-windows client does.

#include "WireGuardConfig.h"
#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <windows.h>

// ── Statistics snapshot (from named-pipe UAPI, optional) ─────────────────────
struct WgStats {
    quint64 rxBytes   = 0;
    quint64 txBytes   = 0;
    QDateTime lastHandshake;
    QString peerPublicKey;
    bool    valid = false;
};

// ── Connection state machine ──────────────────────────────────────────────────
enum class TunnelState {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
};
Q_DECLARE_METATYPE(TunnelState)

// ── Manager ───────────────────────────────────────────────────────────────────
class WireGuardManager : public QObject
{
    Q_OBJECT
public:
    explicit WireGuardManager(QObject* parent = nullptr);
    ~WireGuardManager();

    // Connect using the given config (installs + starts the SCM service)
    void connectTunnel(const WgConfig& cfg);

    // Graceful disconnect (stops + removes service)
    void disconnectTunnel();

    TunnelState state() const { return m_state; }
    WgConfig    activeConfig() const { return m_activeConfig; }
    WgStats     stats() const { return m_stats; }

    // Auto-reconnect settings
    void setAutoReconnect(bool enabled, int maxRetries = 5);

signals:
    void stateChanged(TunnelState newState);
    void statsUpdated(WgStats stats);
    void errorOccurred(const QString& message);
    void logMessage(const QString& msg, bool isError = false);

private slots:
    void onPollTimer();
    void onReconnectTimer();

private:
    // SCM helpers
    bool installService(const WgConfig& cfg, QString& err);
    bool startService(QString& err);
    bool stopService(QString& err);
    bool removeService(QString& err);
    TunnelState queryServiceState();

    // Named-pipe UAPI (wg show equivalent)
    WgStats readStatsFromUAPI();

    void setState(TunnelState s);
    void scheduleReconnect();
    void emit_log(const QString& msg, bool err = false);

    // Conf file written to AppData for the service to read
    QString writeTempConf(const WgConfig& cfg, QString& err);

    TunnelState  m_state = TunnelState::Disconnected;
    WgConfig     m_activeConfig;
    WgStats      m_stats;

    QString      m_serviceName;   // "WireGuardTunnel$<tunnelName>"
    QString      m_confPath;      // temp .conf path given to service

    QTimer*      m_pollTimer     = nullptr;
    QTimer*      m_reconnectTimer= nullptr;

    bool         m_autoReconnect  = true;
    int          m_maxRetries     = 5;
    int          m_retryCount     = 0;

    SC_HANDLE    m_scm            = nullptr;
};
