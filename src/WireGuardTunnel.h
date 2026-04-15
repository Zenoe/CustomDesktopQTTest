#pragma once
// WireGuardTunnel.h
// Low-level interface to tunnel.dll (wireguard-windows)
// tunnel.dll exports: WireGuardGenerateKeypair, WireGuardTunnelService

#include <windows.h>
#include <QString>
#include <QByteArray>
#include <stdexcept>

// ── tunnel.dll calling conventions ────────────────────────────────────────────
// WireGuardTunnelService(confPath: LPCWSTR) -> runs as a service; we call it
//   in a child process elevated via SCM, NOT directly from the GUI process.
// WireGuardGenerateKeypair(publicKey: *[32]byte, privateKey: *[32]byte)
typedef void(__cdecl* PFN_WireGuardGenerateKeypair)(BYTE publicKey[32], BYTE privateKey[32]);
typedef BOOL(__cdecl* PFN_WireGuardTunnelService)(LPCWSTR configFile);

// Base-64 helpers (WireGuard uses standard base64 for keys in .conf files)
namespace WgBase64 {
    QString encode(const BYTE* data, int len);
    QByteArray decode(const QString& b64);
}

// ── DLL wrapper (singleton, lazy-loaded) ─────────────────────────────────────
class TunnelDll
{
public:
    static TunnelDll& instance();

    // Returns base64-encoded public/private key pair
    struct KeyPair { QString publicKey; QString privateKey; };
    KeyPair generateKeypair();

    // Path to the loaded DLL (so we can pass it to the service launcher)
    QString dllPath() const { return m_dllPath; }

    bool isLoaded() const { return m_hDll != nullptr; }

private:
    TunnelDll();
    ~TunnelDll();
    TunnelDll(const TunnelDll&) = delete;
    TunnelDll& operator=(const TunnelDll&) = delete;

    HMODULE m_hDll = nullptr;
    QString m_dllPath;
    PFN_WireGuardGenerateKeypair m_generateKeypair = nullptr;
    PFN_WireGuardTunnelService   m_tunnelService   = nullptr;
};
