// WireGuardTunnel.cpp
#include "WireGuardTunnel.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <stdexcept>

// ── Base64 (RFC 4648, no line breaks) ────────────────────────────────────────
static const char kB64Table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

QString WgBase64::encode(const BYTE* data, int len)
{
    QString out;
    out.reserve(((len + 2) / 3) * 4);
    for (int i = 0; i < len; i += 3) {
        int remain = len - i;
        unsigned b0 = data[i];
        unsigned b1 = remain > 1 ? data[i+1] : 0;
        unsigned b2 = remain > 2 ? data[i+2] : 0;
        out += kB64Table[(b0 >> 2) & 0x3F];
        out += kB64Table[((b0 << 4) | (b1 >> 4)) & 0x3F];
        out += remain > 1 ? kB64Table[((b1 << 2) | (b2 >> 6)) & 0x3F] : '=';
        out += remain > 2 ? kB64Table[b2 & 0x3F] : '=';
    }
    return out;
}

QByteArray WgBase64::decode(const QString& b64)
{
    // Build reverse table
    static signed char rev[256];
    static bool init = false;
    if (!init) {
        memset(rev, -1, sizeof(rev));
        for (int i = 0; i < 64; ++i) rev[(unsigned char)kB64Table[i]] = (signed char)i;
        init = true;
    }
    QByteArray out;
    QByteArray src = b64.toLatin1();
    int i = 0;
    while (i < src.size() && src[i] != '=') {
        if (i + 1 >= src.size()) break;
        unsigned char c0 = rev[(unsigned char)src[i]];
        unsigned char c1 = rev[(unsigned char)src[i+1]];
        out.append(char((c0 << 2) | (c1 >> 4)));
        if (i + 2 < src.size() && src[i+2] != '=') {
            unsigned char c2 = rev[(unsigned char)src[i+2]];
            out.append(char(((c1 & 0xF) << 4) | (c2 >> 2)));
            if (i + 3 < src.size() && src[i+3] != '=') {
                unsigned char c3 = rev[(unsigned char)src[i+3]];
                out.append(char(((c2 & 0x3) << 6) | c3));
            }
        }
        i += 4;
    }
    return out;
}

// ── TunnelDll ─────────────────────────────────────────────────────────────────
TunnelDll& TunnelDll::instance()
{
    static TunnelDll inst;
    return inst;
}

TunnelDll::TunnelDll()
{
    // Search order: exe dir → System32 → PATH
    QStringList searchDirs = {
        QCoreApplication::applicationDirPath(),
        QString(qgetenv("WINDIR")) + "/System32",
    };

    for (const QString& dir : searchDirs) {
        QString candidate = QDir(dir).filePath("tunnel.dll");
        if (QFileInfo::exists(candidate)) {
            m_hDll = ::LoadLibraryW(reinterpret_cast<LPCWSTR>(candidate.utf16()));
            if (m_hDll) {
                m_dllPath = candidate;
                break;
            }
        }
    }

    if (!m_hDll) {
        // Last-resort: let Windows resolve it
        m_hDll = ::LoadLibraryW(L"tunnel.dll");
        if (m_hDll) {
            WCHAR buf[MAX_PATH];
            GetModuleFileNameW(m_hDll, buf, MAX_PATH);
            m_dllPath = QString::fromWCharArray(buf);
        }
    }

    if (!m_hDll) {
        qWarning() << "tunnel.dll not found – keypair generation unavailable";
        return;
    }

    m_generateKeypair = reinterpret_cast<PFN_WireGuardGenerateKeypair>(
        ::GetProcAddress(m_hDll, "WireGuardGenerateKeypair"));
    m_tunnelService = reinterpret_cast<PFN_WireGuardTunnelService>(
        ::GetProcAddress(m_hDll, "WireGuardTunnelService"));

    if (!m_generateKeypair)
        qWarning() << "WireGuardGenerateKeypair not found in tunnel.dll";
    if (!m_tunnelService)
        qWarning() << "WireGuardTunnelService not found in tunnel.dll";
}

TunnelDll::~TunnelDll()
{
    if (m_hDll) ::FreeLibrary(m_hDll);
}

TunnelDll::KeyPair TunnelDll::generateKeypair()
{
    if (!m_generateKeypair)
        throw std::runtime_error("WireGuardGenerateKeypair not available");

    BYTE pub[32] = {}, priv[32] = {};
    m_generateKeypair(pub, priv);
    return { WgBase64::encode(pub, 32), WgBase64::encode(priv, 32) };
}
