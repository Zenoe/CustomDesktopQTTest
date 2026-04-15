// WireGuardServiceHost.cpp
// Thin Windows Service host: SCM calls our exe with --wg-service <conf>.
// We load tunnel.dll and call WireGuardTunnelService() which blocks until stopped.

#include "WireGuardServiceHost.h"
#include <windows.h>
#include <QString>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

// ── Service globals ──────────────────────────────────────────────────────────
static SERVICE_STATUS        g_svcStatus   = {};
static SERVICE_STATUS_HANDLE g_svcHandle   = nullptr;
static QString               g_confPath;

typedef BOOL(__cdecl* PFN_WireGuardTunnelService)(LPCWSTR configFile);
static PFN_WireGuardTunnelService g_fnTunnelService = nullptr;

static void reportStatus(DWORD state, DWORD exitCode = NO_ERROR, DWORD waitHint = 0)
{
    static DWORD checkpoint = 1;
    g_svcStatus.dwCurrentState  = state;
    g_svcStatus.dwWin32ExitCode = exitCode;
    g_svcStatus.dwWaitHint      = waitHint;

    if (state == SERVICE_START_PENDING)
        g_svcStatus.dwControlsAccepted = 0;
    else
        g_svcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

    if (state == SERVICE_RUNNING || state == SERVICE_STOPPED)
        g_svcStatus.dwCheckPoint = 0;
    else
        g_svcStatus.dwCheckPoint = checkpoint++;

    ::SetServiceStatus(g_svcHandle, &g_svcStatus);
}

static DWORD WINAPI serviceCtrlHandler(DWORD ctrl, DWORD, LPVOID, LPVOID)
{
    switch (ctrl) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            reportStatus(SERVICE_STOP_PENDING, NO_ERROR, 5000);
            // WireGuardTunnelService will return when the service is signalled to stop
            // via the standard SCM stop mechanism (it uses an internal event).
            return NO_ERROR;
        default:
            return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

static VOID WINAPI serviceMain(DWORD, LPWSTR*)
{
    g_svcHandle = ::RegisterServiceCtrlHandlerExW(
        L"", serviceCtrlHandler, nullptr);
    if (!g_svcHandle) return;

    g_svcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    reportStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    if (!g_fnTunnelService) {
        reportStatus(SERVICE_STOPPED, ERROR_DLL_NOT_FOUND);
        return;
    }

    reportStatus(SERVICE_RUNNING);

    // This call BLOCKS until the tunnel is stopped by SCM
    g_fnTunnelService(reinterpret_cast<LPCWSTR>(g_confPath.utf16()));

    reportStatus(SERVICE_STOPPED);
}

// ── Public API ────────────────────────────────────────────────────────────────
bool handleServiceMode(int argc, char* argv[])
{
    // Check for --wg-service <confPath> argument
    for (int i = 1; i < argc - 1; ++i) {
        if (QString(argv[i]) == "--wg-service") {
            g_confPath = QString::fromLocal8Bit(argv[i + 1]);
            break;
        }
    }
    if (g_confPath.isEmpty()) return false;

    // Load tunnel.dll
    QStringList searchDirs = {
        QDir(QString::fromLocal8Bit(argv[0])).absolutePath(),
        QString(qgetenv("WINDIR")) + "/System32"
    };

    HMODULE hDll = nullptr;
    for (const QString& dir : searchDirs) {
        QString path = QDir(dir).filePath("tunnel.dll");
        if (QFileInfo::exists(path)) {
            hDll = ::LoadLibraryW(reinterpret_cast<LPCWSTR>(path.utf16()));
            if (hDll) break;
        }
    }
    if (!hDll) hDll = ::LoadLibraryW(L"tunnel.dll");

    if (hDll) {
        g_fnTunnelService = reinterpret_cast<PFN_WireGuardTunnelService>(
            ::GetProcAddress(hDll, "WireGuardTunnelService"));
    }

    // Enter SCM service dispatcher – this blocks until stopped
    SERVICE_TABLE_ENTRYW table[] = {
        { const_cast<LPWSTR>(L""), serviceMain },
        { nullptr, nullptr }
    };
    ::StartServiceCtrlDispatcherW(table);

    if (hDll) ::FreeLibrary(hDll);
    return true;   // we handled it; caller should exit
}
