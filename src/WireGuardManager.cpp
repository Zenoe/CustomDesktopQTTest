// WireGuardManager.cpp
#include "WireGuardManager.h"
#include "WireGuardTunnel.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QThread>

// ── Service name convention (matches wireguard-windows) ──────────────────────
static QString serviceNameFor(const QString& tunnelName)
{
    // wireguard-windows uses "WireGuardTunnel$<name>"
    return "WireGuardTunnel$" + tunnelName;
}

// ── UAPI pipe name (for stats) ────────────────────────────────────────────────
static QString uapiPipeName(const QString& tunnelName)
{
    return R"(\\.\pipe\ProtectedPrefix\Administrators\WireGuard\)" + tunnelName;
}

// ─────────────────────────────────────────────────────────────────────────────
WireGuardManager::WireGuardManager(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<TunnelState>();

    m_scm = ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!m_scm)
        emit_log("OpenSCManager failed – administrator rights may be required", true);

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(2000);
    connect(m_pollTimer, &QTimer::timeout, this, &WireGuardManager::onPollTimer);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &WireGuardManager::onReconnectTimer);
}

WireGuardManager::~WireGuardManager()
{
    if (m_state == TunnelState::Connected || m_state == TunnelState::Connecting)
        disconnectTunnel();
    if (m_scm) ::CloseServiceHandle(m_scm);
}

// ── Public API ────────────────────────────────────────────────────────────────
void WireGuardManager::connectTunnel(const WgConfig& cfg)
{
    if (m_state == TunnelState::Connected || m_state == TunnelState::Connecting) {
        emit_log("Already connected – disconnect first");
        return;
    }

    m_retryCount   = 0;
    m_activeConfig = cfg;
    m_serviceName  = serviceNameFor(cfg.name);

    setState(TunnelState::Connecting);
    emit_log("Connecting tunnel: " + cfg.name);

    QString err;
    m_confPath = writeTempConf(cfg, err);
    if (m_confPath.isEmpty()) {
        emit errorOccurred("Failed to write config: " + err);
        setState(TunnelState::Error);
        return;
    }

    // Stop + remove any stale instance
    stopService(err);
    removeService(err);

    if (!installService(cfg, err)) {
        emit errorOccurred("Service install failed: " + err);
        setState(TunnelState::Error);
        return;
    }

    if (!startService(err)) {
        emit errorOccurred("Service start failed: " + err);
        removeService(err);
        setState(TunnelState::Error);
        return;
    }

    // Start polling; state will flip to Connected once SCM reports Running
    m_pollTimer->start();
    emit_log("Tunnel service started – waiting for handshake…");
}

void WireGuardManager::disconnectTunnel()
{
    m_reconnectTimer->stop();
    m_autoReconnect = false;   // suppress reconnect on deliberate disconnect

    setState(TunnelState::Disconnecting);
    emit_log("Disconnecting tunnel: " + m_activeConfig.name);

    QString err;
    stopService(err);
    removeService(err);

    // Clean up temp conf
    if (!m_confPath.isEmpty()) {
        QFile::remove(m_confPath);
        m_confPath.clear();
    }

    m_pollTimer->stop();
    setState(TunnelState::Disconnected);
    emit_log("Tunnel disconnected");
}

void WireGuardManager::setAutoReconnect(bool enabled, int maxRetries)
{
    m_autoReconnect = enabled;
    m_maxRetries    = maxRetries;
}

// ── SCM helpers ───────────────────────────────────────────────────────────────
QString WireGuardManager::writeTempConf(const WgConfig& cfg, QString& err)
{
    // Store in %ProgramData%\WireGuard (where the service expects it)
    // Fallback: AppData
    QString base = "C:/ProgramData/WireGuard";
    if (!QDir().mkpath(base))
        base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/wireguard";
    QDir().mkpath(base);

    QString path = base + "/" + cfg.name + ".conf";
    if (!cfg.saveToFile(path, err))
        return {};
    return path;
}

bool WireGuardManager::installService(const WgConfig& cfg, QString& err)
{
    if (!m_scm) { err = "SCM not open"; return false; }

    // Binary path: "tunnel.dll" is loaded by a thin launcher.
    // The official approach is to use the WireGuard service executable.
    // We use the same scheme: create a service whose binary is our own exe
    // with a special argument, OR use rundll32 on tunnel.dll entry point.
    //
    // Best practice (matching official client): the service binary IS tunnel.dll
    // invoked via a minimal native service host (wireguard.exe /service <conf>).
    // We embed a resource-free service launcher: our app itself handles
    // service mode if started with --wg-service <conf> argument.

    QString dllPath = TunnelDll::instance().dllPath();
    QString exePath = QCoreApplication::applicationFilePath();
    exePath.replace('/', '\\');

    // Service binary: our own exe acting as service host
    QString binPath = QString("\"%1\" --wg-service \"%2\"")
        .arg(exePath, m_confPath);

    SC_HANDLE svc = ::CreateServiceW(
        m_scm,
        reinterpret_cast<LPCWSTR>(m_serviceName.utf16()),
        reinterpret_cast<LPCWSTR>(("WireGuard Tunnel: " + cfg.name).utf16()),
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        reinterpret_cast<LPCWSTR>(binPath.utf16()),
        nullptr,    // no load-order group
        nullptr,    // no tag id
        L"Nsi\0TcpIp\0",  // dependencies
        nullptr,    // LocalSystem
        nullptr
    );

    if (!svc) {
        DWORD e = GetLastError();
        if (e == ERROR_SERVICE_EXISTS) return true; // already installed
        err = QString("CreateService error %1").arg(e);
        return false;
    }

    // Set description
    SERVICE_DESCRIPTIONW desc;
    QString descStr = "WireGuard Tunnel managed by CustomDesktopQt";
    desc.lpDescription = reinterpret_cast<LPWSTR>(descStr.data());
    ::ChangeServiceConfig2W(svc, SERVICE_CONFIG_DESCRIPTION, &desc);

    ::CloseServiceHandle(svc);
    emit_log("Service installed: " + m_serviceName);
    return true;
}

bool WireGuardManager::startService(QString& err)
{
    if (!m_scm) { err = "SCM not open"; return false; }

    SC_HANDLE svc = ::OpenServiceW(m_scm,
        reinterpret_cast<LPCWSTR>(m_serviceName.utf16()),
        SERVICE_START | SERVICE_QUERY_STATUS);
    if (!svc) { err = QString("OpenService error %1").arg(GetLastError()); return false; }

    BOOL ok = ::StartServiceW(svc, 0, nullptr);
    DWORD e = GetLastError();
    ::CloseServiceHandle(svc);

    if (!ok && e != ERROR_SERVICE_ALREADY_RUNNING) {
        err = QString("StartService error %1").arg(e);
        return false;
    }
    return true;
}

bool WireGuardManager::stopService(QString& err)
{
    if (!m_scm) { err = "SCM not open"; return false; }

    SC_HANDLE svc = ::OpenServiceW(m_scm,
        reinterpret_cast<LPCWSTR>(m_serviceName.utf16()),
        SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!svc) return true; // doesn't exist, fine

    SERVICE_STATUS ss;
    ::ControlService(svc, SERVICE_CONTROL_STOP, &ss);

    // Wait up to 10 s for stop
    for (int i = 0; i < 50; ++i) {
        if (!::QueryServiceStatus(svc, &ss)) break;
        if (ss.dwCurrentState == SERVICE_STOPPED) break;
        ::Sleep(200);
    }
    ::CloseServiceHandle(svc);
    return true;
}

bool WireGuardManager::removeService(QString& err)
{
    if (!m_scm) { err = "SCM not open"; return false; }

    SC_HANDLE svc = ::OpenServiceW(m_scm,
        reinterpret_cast<LPCWSTR>(m_serviceName.utf16()),
        DELETE);
    if (!svc) return true;

    ::DeleteService(svc);
    ::CloseServiceHandle(svc);
    return true;
}

TunnelState WireGuardManager::queryServiceState()
{
    if (!m_scm) return TunnelState::Error;

    SC_HANDLE svc = ::OpenServiceW(m_scm,
        reinterpret_cast<LPCWSTR>(m_serviceName.utf16()),
        SERVICE_QUERY_STATUS);
    if (!svc) return TunnelState::Disconnected;

    SERVICE_STATUS ss;
    if (!::QueryServiceStatus(svc, &ss)) {
        ::CloseServiceHandle(svc);
        return TunnelState::Error;
    }
    ::CloseServiceHandle(svc);

    switch (ss.dwCurrentState) {
        case SERVICE_RUNNING:      return TunnelState::Connected;
        case SERVICE_START_PENDING:return TunnelState::Connecting;
        case SERVICE_STOP_PENDING: return TunnelState::Disconnecting;
        case SERVICE_STOPPED:      return TunnelState::Disconnected;
        default:                   return TunnelState::Disconnected;
    }
}

// ── UAPI stats via named pipe ─────────────────────────────────────────────────
WgStats WireGuardManager::readStatsFromUAPI()
{
    WgStats stats;
    QString pipeName = uapiPipeName(m_activeConfig.name);

    HANDLE pipe = ::CreateFileW(
        reinterpret_cast<LPCWSTR>(pipeName.utf16()),
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED, nullptr);

    if (pipe == INVALID_HANDLE_VALUE) return stats;

    // Send UAPI "get=1\n\n" request
    const char req[] = "get=1\n\n";
    DWORD written;
    if (!::WriteFile(pipe, req, sizeof(req)-1, &written, nullptr)) {
        ::CloseHandle(pipe);
        return stats;
    }

    // Read response
    QByteArray response;
    char buf[4096];
    DWORD read;
    while (::ReadFile(pipe, buf, sizeof(buf), &read, nullptr) && read > 0)
        response.append(buf, read);
    ::CloseHandle(pipe);

    // Parse key=value pairs
    for (const QByteArray& line : response.split('\n')) {
        QString kv = QString::fromUtf8(line).trimmed();
        int eq = kv.indexOf('=');
        if (eq < 0) continue;
        QString k = kv.left(eq);
        QString v = kv.mid(eq+1);

        if      (k == "rx_bytes")        stats.rxBytes = v.toULongLong();
        else if (k == "tx_bytes")        stats.txBytes = v.toULongLong();
        else if (k == "last_handshake_time_sec")
            stats.lastHandshake = QDateTime::fromSecsSinceEpoch(v.toLongLong(), Qt::UTC);
        else if (k == "public_key")      stats.peerPublicKey = v;
    }
    stats.valid = true;
    return stats;
}

// ── Slots ─────────────────────────────────────────────────────────────────────
void WireGuardManager::onPollTimer()
{
    TunnelState svcState = queryServiceState();

    if (svcState == TunnelState::Connected) {
        setState(TunnelState::Connected);
        m_stats = readStatsFromUAPI();
        emit statsUpdated(m_stats);
        m_retryCount = 0;
    } else if (svcState == TunnelState::Disconnected &&
               m_state == TunnelState::Connected)
    {
        // Unexpected drop
        emit_log("Tunnel dropped unexpectedly", true);
        setState(TunnelState::Error);
        m_pollTimer->stop();
        if (m_autoReconnect) scheduleReconnect();
    } else if (svcState == TunnelState::Error) {
        emit errorOccurred("Service entered error state");
        setState(TunnelState::Error);
        m_pollTimer->stop();
        if (m_autoReconnect) scheduleReconnect();
    }
    // Connecting → keep polling
}

void WireGuardManager::onReconnectTimer()
{
    if (m_retryCount >= m_maxRetries) {
        emit errorOccurred(QString("Auto-reconnect failed after %1 attempts").arg(m_retryCount));
        setState(TunnelState::Disconnected);
        return;
    }
    ++m_retryCount;
    emit_log(QString("Auto-reconnect attempt %1/%2…").arg(m_retryCount).arg(m_maxRetries));

    // Re-enable auto-reconnect flag (was cleared in connectTunnel path)
    bool savedAR = m_autoReconnect;
    m_autoReconnect = true;
    connectTunnel(m_activeConfig);
    m_autoReconnect = savedAR;
}

// ── Internals ─────────────────────────────────────────────────────────────────
void WireGuardManager::setState(TunnelState s)
{
    if (m_state == s) return;
    m_state = s;
    emit stateChanged(s);
}

void WireGuardManager::scheduleReconnect()
{
    // Exponential backoff: 5s, 10s, 20s, 40s, 60s max
    int delay = qMin(5000 * (1 << qMin(m_retryCount, 3)), 60000);
    emit_log(QString("Reconnecting in %1 s…").arg(delay / 1000));
    m_reconnectTimer->start(delay);
}

void WireGuardManager::emit_log(const QString& msg, bool err)
{
    emit logMessage(msg, err);
    if (err) qWarning() << "[WireGuard]" << msg;
    else     qDebug()   << "[WireGuard]" << msg;
}
