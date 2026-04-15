// WireGuardConfig.cpp
#include "WireGuardConfig.h"
#include "WireGuardTunnel.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

// ── helpers ──────────────────────────────────────────────────────────────────
static QString stripped(const QString& s) { return s.trimmed(); }

static void appendKV(QString& out, const QString& k, const QString& v)
{
    if (!v.isEmpty()) out += k + " = " + v + "\n";
}

// ── WgConfig::toConfString ───────────────────────────────────────────────────
QString WgConfig::toConfString() const
{
    QString out;

    out += "[Interface]\n";
    appendKV(out, "PrivateKey", iface.privateKey);
    if (!iface.addresses.isEmpty())
        appendKV(out, "Address", iface.addresses.join(", "));
    if (iface.listenPort)
        appendKV(out, "ListenPort", QString::number(iface.listenPort));
    if (!iface.dns.isEmpty())
        appendKV(out, "DNS", iface.dns.join(", "));
    if (!iface.mtu.isEmpty())
        appendKV(out, "MTU", iface.mtu);
    if (!iface.table.isEmpty())
        appendKV(out, "Table", iface.table);
    for (const auto& s : iface.preUp)   appendKV(out, "PreUp",   s);
    for (const auto& s : iface.postUp)  appendKV(out, "PostUp",  s);
    for (const auto& s : iface.preDown) appendKV(out, "PreDown", s);
    for (const auto& s : iface.postDown)appendKV(out, "PostDown",s);

    for (const WgPeer& p : peers) {
        out += "\n[Peer]\n";
        if (!p.name.isEmpty())
            out += "# Name = " + p.name + "\n";
        appendKV(out, "PublicKey",           p.publicKey);
        appendKV(out, "PreSharedKey",        p.preSharedKey);
        appendKV(out, "Endpoint",            p.endpoint);
        if (!p.allowedIPs.isEmpty())
            appendKV(out, "AllowedIPs", p.allowedIPs.join(", "));
        if (p.persistentKeepalive > 0)
            appendKV(out, "PersistentKeepalive", QString::number(p.persistentKeepalive));
    }
    return out;
}

// ── WgConfig::fromConfString ──────────────────────────────────────────────────
bool WgConfig::fromConfString(const QString& text, QString& errorMsg)
{
    iface = {};
    peers.clear();

    enum Section { None, Interface, Peer } section = None;
    WgPeer currentPeer;

    auto commitPeer = [&]() {
        if (section == Peer && !currentPeer.publicKey.isEmpty())
            peers.append(currentPeer);
        currentPeer = {};
    };

    static QRegularExpression reSectionHeader(R"(^\[(\w+)\]\s*$)");
    static QRegularExpression reKV(R"(^([^=]+?)\s*=\s*(.*)$)");
    static QRegularExpression reComment(R"(^#\s*Name\s*=\s*(.+)$)");

    for (const QString& rawLine : text.split('\n')) {
        QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;

        // Named comment inside [Peer]
        QRegularExpressionMatch cm = reComment.match(line);
        if (cm.hasMatch() && section == Peer) {
            currentPeer.name = cm.captured(1).trimmed();
            continue;
        }

        if (line.startsWith('#')) continue;  // generic comment

        QRegularExpressionMatch sm = reSectionHeader.match(line);
        if (sm.hasMatch()) {
            commitPeer();
            QString sec = sm.captured(1).toLower();
            if      (sec == "interface") section = Interface;
            else if (sec == "peer")      section = Peer;
            else { errorMsg = "Unknown section: " + sec; return false; }
            continue;
        }

        QRegularExpressionMatch kvm = reKV.match(line);
        if (!kvm.hasMatch()) continue;

        QString key = kvm.captured(1).trimmed().toLower();
        QString val = kvm.captured(2).trimmed();

        // Strip inline comments
        {
            int hash = val.indexOf('#');
            if (hash >= 0) val = val.left(hash).trimmed();
        }

        if (section == Interface) {
            if      (key == "privatekey")  iface.privateKey  = val;
            else if (key == "address")     iface.addresses   = val.split(',', Qt::SkipEmptyParts);
            else if (key == "listenport")  iface.listenPort  = val.toUShort();
            else if (key == "dns")         iface.dns         = val.split(',', Qt::SkipEmptyParts);
            else if (key == "mtu")         iface.mtu         = val;
            else if (key == "table")       iface.table       = val;
            else if (key == "preup")       iface.preUp       << val;
            else if (key == "postup")      iface.postUp      << val;
            else if (key == "predown")     iface.preDown     << val;
            else if (key == "postdown")    iface.postDown    << val;
        } else if (section == Peer) {
            if      (key == "publickey")            currentPeer.publicKey       = val;
            else if (key == "presharedkey")         currentPeer.preSharedKey    = val;
            else if (key == "endpoint")             currentPeer.endpoint        = val;
            else if (key == "allowedips")           currentPeer.allowedIPs      = val.split(',', Qt::SkipEmptyParts);
            else if (key == "persistentkeepalive")  currentPeer.persistentKeepalive = val.toInt();
        }
    }
    commitPeer();

    // Trim whitespace in lists
    for (auto& addr : iface.addresses) addr = addr.trimmed();
    for (auto& d    : iface.dns)       d    = d.trimmed();
    for (auto& p    : peers) {
        for (auto& ip : p.allowedIPs) ip = ip.trimmed();
    }

    if (iface.privateKey.isEmpty()) {
        errorMsg = "No PrivateKey found in [Interface]";
        return false;
    }

    // Derive public key via tunnel.dll if available
    try {
        auto& dll = TunnelDll::instance();
        if (dll.isLoaded()) {
            // We can't derive pubkey from privkey without wg-go crypto exposed,
            // so we store a note; the official client derives it internally.
            // (WireGuardGenerateKeypair generates a NEW keypair, not derives.)
            iface.publicKey = "(derived by service)";
        }
    } catch (...) {}

    return true;
}

// ── static helpers ────────────────────────────────────────────────────────────
WgConfig WgConfig::fromFile(const QString& path, QString& errorMsg)
{
    WgConfig cfg;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMsg = "Cannot open file: " + path;
        return cfg;
    }
    QString text = QString::fromUtf8(f.readAll());
    cfg.filePath = path;
    cfg.name = QFileInfo(path).completeBaseName();
    if (!cfg.fromConfString(text, errorMsg))
        cfg = {};
    return cfg;
}

bool WgConfig::saveToFile(const QString& path, QString& errorMsg) const
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        errorMsg = "Cannot write file: " + path;
        return false;
    }
    // Restrict permissions: only owner can read (mimics wg-quick)
    f.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    QTextStream ts(&f);
    ts << toConfString();
    return true;
}

bool WgConfig::isValid() const
{
    return !iface.privateKey.isEmpty() && !peers.isEmpty();
}
