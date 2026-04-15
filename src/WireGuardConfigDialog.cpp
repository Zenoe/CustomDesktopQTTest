// WireGuardConfigDialog.cpp
#include "WireGuardConfigDialog.h"
#include "WireGuardTunnel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QToolButton>
#include <QTabWidget>
#include <QScrollArea>
#include <QApplication>
#include <QClipboard>

// ── Ctor ──────────────────────────────────────────────────────────────────────
WireGuardConfigDialog::WireGuardConfigDialog(QWidget* parent, const WgConfig& existing)
    : QDialog(parent), m_config(existing)
{
    setWindowTitle("WireGuard Configuration");
    setMinimumSize(740, 580);
    buildUI();
    if (!existing.name.isEmpty())
        populateFromConfig(existing);
}

// ── UI construction ───────────────────────────────────────────────────────────
void WireGuardConfigDialog::buildUI()
{
    auto* root = new QVBoxLayout(this);

    // Toolbar row
    auto* toolbar = new QHBoxLayout;
    auto* btnImport = new QPushButton("📂  Import .conf…");
    auto* btnExport = new QPushButton("💾  Export .conf…");
    toolbar->addWidget(btnImport);
    toolbar->addWidget(btnExport);
    toolbar->addStretch();
    root->addLayout(toolbar);

    // Tabs: Form / Raw
    auto* tabs = new QTabWidget;

    // ── Form tab ─────────────────────────────────────────────────────────────
    auto* formWidget = new QWidget;
    auto* formRoot   = new QHBoxLayout(formWidget);

    // Left: Interface
    auto* ifaceBox = new QGroupBox("Interface");
    auto* ifaceForm = new QFormLayout(ifaceBox);
    ifaceForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_editName    = new QLineEdit;
    m_editName->setPlaceholderText("MyTunnel");
    m_editPrivKey = new QLineEdit;
    m_editPrivKey->setEchoMode(QLineEdit::Password);
    m_editPrivKey->setPlaceholderText("base64 private key");
    m_lblPubKey   = new QLabel("(auto-derived)");
    m_lblPubKey->setStyleSheet("color: gray; font-size: 10px;");
    m_editAddresses = new QLineEdit;
    m_editAddresses->setPlaceholderText("10.0.0.1/24, fd00::1/128");
    m_spinPort    = new QSpinBox; m_spinPort->setRange(0, 65535); m_spinPort->setSpecialValueText("(random)");
    m_editDNS     = new QLineEdit; m_editDNS->setPlaceholderText("1.1.1.1, 1.0.0.1");
    m_editMTU     = new QLineEdit; m_editMTU->setPlaceholderText("1420 (default)");

    // Key row with generate button
    auto* keyRow = new QHBoxLayout;
    keyRow->addWidget(m_editPrivKey);
    auto* btnGen = new QPushButton("⚙ Generate");
    btnGen->setToolTip("Generate new keypair via tunnel.dll");
    keyRow->addWidget(btnGen);

    ifaceForm->addRow("Tunnel Name:",   m_editName);
    ifaceForm->addRow("Private Key:",   keyRow);
    ifaceForm->addRow("Public Key:",    m_lblPubKey);
    ifaceForm->addRow("Addresses:",     m_editAddresses);
    ifaceForm->addRow("Listen Port:",   m_spinPort);
    ifaceForm->addRow("DNS Servers:",   m_editDNS);
    ifaceForm->addRow("MTU:",           m_editMTU);

    // Right: Peers
    auto* peerBox  = new QGroupBox("Peers");
    auto* peerRoot = new QVBoxLayout(peerBox);

    auto* peerHeader = new QHBoxLayout;
    auto* btnAddPeer = new QPushButton("＋ Add Peer");
    m_btnRemPeer     = new QPushButton("－ Remove");
    m_btnRemPeer->setEnabled(false);
    peerHeader->addWidget(btnAddPeer);
    peerHeader->addWidget(m_btnRemPeer);
    peerHeader->addStretch();

    m_peerList = new QListWidget;
    m_peerList->setMaximumHeight(130);

    // Peer editor fields
    auto* peerForm = new QFormLayout;
    m_peerName       = new QLineEdit; m_peerName->setPlaceholderText("Friendly name");
    m_peerPubKey     = new QLineEdit; m_peerPubKey->setPlaceholderText("base64 public key");
    m_peerPSK        = new QLineEdit; m_peerPSK->setEchoMode(QLineEdit::Password);
    m_peerPSK->setPlaceholderText("optional");
    m_peerEndpoint   = new QLineEdit; m_peerEndpoint->setPlaceholderText("vpn.example.com:51820");
    m_peerAllowedIPs = new QLineEdit; m_peerAllowedIPs->setPlaceholderText("0.0.0.0/0, ::/0");
    m_peerKeepalive  = new QSpinBox; m_peerKeepalive->setRange(0, 65535);
    m_peerKeepalive->setSpecialValueText("disabled");
    m_peerKeepalive->setSuffix(" s");

    peerForm->addRow("Name:",          m_peerName);
    peerForm->addRow("Public Key:",    m_peerPubKey);
    peerForm->addRow("Pre-Shared Key:",m_peerPSK);
    peerForm->addRow("Endpoint:",      m_peerEndpoint);
    peerForm->addRow("Allowed IPs:",   m_peerAllowedIPs);
    peerForm->addRow("Keepalive:",     m_peerKeepalive);

    peerRoot->addLayout(peerHeader);
    peerRoot->addWidget(m_peerList);
    peerRoot->addSpacing(6);
    peerRoot->addLayout(peerForm);
    peerRoot->addStretch();

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(ifaceBox);
    splitter->addWidget(peerBox);
    splitter->setSizes({320, 400});
    formRoot->addWidget(splitter);

    tabs->addTab(formWidget, "Visual Editor");

    // ── Raw tab ──────────────────────────────────────────────────────────────
    m_rawEdit = new QTextEdit;
    m_rawEdit->setFontFamily("Consolas");
    m_rawEdit->setPlaceholderText("Paste or type a raw .conf here…");
    tabs->addTab(m_rawEdit, "Raw Config");

    root->addWidget(tabs);

    // ── Buttons ───────────────────────────────────────────────────────────────
    auto* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    root->addWidget(bbox);

    // Connections
    connect(btnImport,  &QPushButton::clicked, this, &WireGuardConfigDialog::onImportConf);
    connect(btnExport,  &QPushButton::clicked, this, &WireGuardConfigDialog::onExportConf);
    connect(btnGen,     &QPushButton::clicked, this, &WireGuardConfigDialog::onGenerateKeypair);
    connect(btnAddPeer, &QPushButton::clicked, this, &WireGuardConfigDialog::onAddPeer);
    connect(m_btnRemPeer,&QPushButton::clicked,this, &WireGuardConfigDialog::onRemovePeer);
    connect(m_peerList, &QListWidget::currentRowChanged, this, &WireGuardConfigDialog::onPeerSelected);
    connect(bbox,       &QDialogButtonBox::accepted, this, &WireGuardConfigDialog::onAccept);
    connect(bbox,       &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Sync raw ↔ form on tab switch
    connect(tabs, &QTabWidget::currentChanged, this, [this, tabs](int idx){
        if (idx == 1) {
            // Going to Raw: serialise current form state
            m_rawEdit->setPlainText(collectConfig().toConfString());
        } else {
            // Coming back: try to parse raw into form
            WgConfig tmp;
            QString err;
            if (tmp.fromConfString(m_rawEdit->toPlainText(), err))
                populateFromConfig(tmp);
        }
    });
}

// ── Populate form from config ─────────────────────────────────────────────────
void WireGuardConfigDialog::populateFromConfig(const WgConfig& cfg)
{
    m_config = cfg;
    m_editName->setText(cfg.name);
    m_editPrivKey->setText(cfg.iface.privateKey);
    m_lblPubKey->setText(cfg.iface.publicKey.isEmpty() ? "(auto-derived)" : cfg.iface.publicKey);
    m_editAddresses->setText(cfg.iface.addresses.join(", "));
    m_spinPort->setValue(cfg.iface.listenPort);
    m_editDNS->setText(cfg.iface.dns.join(", "));
    m_editMTU->setText(cfg.iface.mtu);
    updatePeerList();
}

// ── Collect form → WgConfig ───────────────────────────────────────────────────
WgConfig WireGuardConfigDialog::collectConfig() const
{
    // Flush any in-progress peer edits back to m_config
    WireGuardConfigDialog* self = const_cast<WireGuardConfigDialog*>(this);
    if (m_currentPeerIdx >= 0 && m_currentPeerIdx < m_config.peers.size()) {
        WgPeer& p = self->m_config.peers[m_currentPeerIdx];
        p.name              = m_peerName->text().trimmed();
        p.publicKey         = m_peerPubKey->text().trimmed();
        p.preSharedKey      = m_peerPSK->text().trimmed();
        p.endpoint          = m_peerEndpoint->text().trimmed();
        p.allowedIPs        = m_peerAllowedIPs->text().split(',', Qt::SkipEmptyParts);
        for (auto& ip : p.allowedIPs) ip = ip.trimmed();
        p.persistentKeepalive = m_peerKeepalive->value();
    }

    WgConfig cfg = m_config;
    cfg.name                 = m_editName->text().trimmed();
    cfg.iface.privateKey     = m_editPrivKey->text().trimmed();
    cfg.iface.addresses      = m_editAddresses->text().split(',', Qt::SkipEmptyParts);
    for (auto& a : cfg.iface.addresses) a = a.trimmed();
    cfg.iface.listenPort     = (quint16)m_spinPort->value();
    cfg.iface.dns            = m_editDNS->text().split(',', Qt::SkipEmptyParts);
    for (auto& d : cfg.iface.dns) d = d.trimmed();
    cfg.iface.mtu            = m_editMTU->text().trimmed();
    return cfg;
}

// ── Peer list management ──────────────────────────────────────────────────────
void WireGuardConfigDialog::updatePeerList()
{
    m_peerList->clear();
    for (const WgPeer& p : m_config.peers) {
        QString label = p.name.isEmpty() ? p.publicKey.left(16) + "…" : p.name;
        m_peerList->addItem(label);
    }
    m_currentPeerIdx = -1;
    m_btnRemPeer->setEnabled(false);
}

void WireGuardConfigDialog::onPeerSelected(int index)
{
    // Save previous
    if (m_currentPeerIdx >= 0 && m_currentPeerIdx < m_config.peers.size()) {
        WgPeer& p = m_config.peers[m_currentPeerIdx];
        p.name              = m_peerName->text().trimmed();
        p.publicKey         = m_peerPubKey->text().trimmed();
        p.preSharedKey      = m_peerPSK->text().trimmed();
        p.endpoint          = m_peerEndpoint->text().trimmed();
        p.allowedIPs        = m_peerAllowedIPs->text().split(',', Qt::SkipEmptyParts);
        for (auto& ip : p.allowedIPs) ip = ip.trimmed();
        p.persistentKeepalive = m_peerKeepalive->value();
    }

    m_currentPeerIdx = index;
    if (index < 0 || index >= m_config.peers.size()) {
        m_btnRemPeer->setEnabled(false);
        return;
    }

    const WgPeer& p = m_config.peers[index];
    m_peerName->setText(p.name);
    m_peerPubKey->setText(p.publicKey);
    m_peerPSK->setText(p.preSharedKey);
    m_peerEndpoint->setText(p.endpoint);
    m_peerAllowedIPs->setText(p.allowedIPs.join(", "));
    m_peerKeepalive->setValue(p.persistentKeepalive);
    m_btnRemPeer->setEnabled(true);
}

// ── Slots ─────────────────────────────────────────────────────────────────────
void WireGuardConfigDialog::onImportConf()
{
    QString path = QFileDialog::getOpenFileName(this,
        "Import WireGuard Config", {}, "WireGuard Config (*.conf);;All Files (*)");
    if (path.isEmpty()) return;

    QString err;
    WgConfig cfg = WgConfig::fromFile(path, err);
    if (cfg.iface.privateKey.isEmpty()) {
        QMessageBox::critical(this, "Import Error", err.isEmpty() ? "Invalid config file" : err);
        return;
    }
    populateFromConfig(cfg);
}

void WireGuardConfigDialog::onExportConf()
{
    WgConfig cfg = collectConfig();
    if (cfg.name.isEmpty()) { QMessageBox::warning(this, "Export", "Set a tunnel name first"); return; }

    QString path = QFileDialog::getSaveFileName(this,
        "Export WireGuard Config", cfg.name + ".conf",
        "WireGuard Config (*.conf);;All Files (*)");
    if (path.isEmpty()) return;

    QString err;
    if (!cfg.saveToFile(path, err))
        QMessageBox::critical(this, "Export Error", err);
    else
        QMessageBox::information(this, "Exported", "Config saved to:\n" + path);
}

void WireGuardConfigDialog::onGenerateKeypair()
{
    try {
        auto kp = TunnelDll::instance().generateKeypair();
        m_editPrivKey->setText(kp.privateKey);
        m_lblPubKey->setText(kp.publicKey);
        m_lblPubKey->setStyleSheet("color: green; font-size: 10px;");

        // Offer to copy pubkey
        if (QMessageBox::question(this, "Keypair Generated",
            "New keypair generated.\n\nPublic key copied to clipboard?\n\n" + kp.publicKey,
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        {
            QApplication::clipboard()->setText(kp.publicKey);
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", QString("Key generation failed: %1\n\n"
            "Ensure tunnel.dll is in the application directory.").arg(e.what()));
    }
}

void WireGuardConfigDialog::onAddPeer()
{
    // Save current
    onPeerSelected(m_currentPeerIdx);

    WgPeer p;
    p.name = QString("Peer %1").arg(m_config.peers.size() + 1);
    p.allowedIPs << "0.0.0.0/0" << "::/0";
    m_config.peers.append(p);
    updatePeerList();
    m_peerList->setCurrentRow(m_config.peers.size() - 1);
}

void WireGuardConfigDialog::onRemovePeer()
{
    if (m_currentPeerIdx < 0 || m_currentPeerIdx >= m_config.peers.size()) return;
    if (QMessageBox::question(this, "Remove Peer",
        "Remove peer '" + m_config.peers[m_currentPeerIdx].name + "'?",
        QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    m_config.peers.removeAt(m_currentPeerIdx);
    m_currentPeerIdx = -1;
    updatePeerList();
}

void WireGuardConfigDialog::onAccept()
{
    onPeerSelected(m_currentPeerIdx); // flush edits
    m_config = collectConfig();

    if (m_config.name.isEmpty()) {
        QMessageBox::warning(this, "Validation", "Tunnel name is required.");
        return;
    }
    if (m_config.iface.privateKey.isEmpty()) {
        QMessageBox::warning(this, "Validation", "Private key is required.");
        return;
    }
    if (m_config.peers.isEmpty()) {
        if (QMessageBox::question(this, "No Peers",
            "No peers configured. Save anyway?",
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
            return;
    }
    accept();
}
