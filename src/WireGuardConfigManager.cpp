// WireGuardConfigManager.cpp
#include "WireGuardConfigManager.h"
#include "WireGuardConfigDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QDialogButtonBox>

WireGuardConfigManager::WireGuardConfigManager(WireGuardManager* mgr, QWidget* parent)
    : QDialog(parent), m_mgr(mgr)
{
    setWindowTitle("WireGuard – Manage Tunnels");
    setMinimumSize(560, 400);

    auto* root   = new QVBoxLayout(this);

    // Header
    auto* header = new QLabel("Configured Tunnels");
    QFont f = header->font(); f.setPointSize(12); f.setBold(true);
    header->setFont(f);
    root->addWidget(header);

    // List + side buttons
    auto* row = new QHBoxLayout;
    m_list = new QListWidget;
    m_list->setAlternatingRowColors(true);
    row->addWidget(m_list);

    auto* btns = new QVBoxLayout;
    auto* btnAdd    = new QPushButton("＋ New");
    m_btnEdit       = new QPushButton("✏ Edit");
    m_btnRemove     = new QPushButton("🗑 Remove");
    auto* btnImport = new QPushButton("📂 Import…");
    m_btnConnect    = new QPushButton("⚡ Connect");
    m_btnConnect->setStyleSheet("font-weight: bold; background: #27ae60; color: white; padding: 6px;");
    auto* btnClose  = new QPushButton("Close");

    m_btnEdit->setEnabled(false);
    m_btnRemove->setEnabled(false);
    m_btnConnect->setEnabled(false);

    btns->addWidget(btnAdd);
    btns->addWidget(m_btnEdit);
    btns->addWidget(m_btnRemove);
    btns->addWidget(btnImport);
    btns->addSpacing(12);
    btns->addWidget(m_btnConnect);
    btns->addStretch();
    btns->addWidget(btnClose);
    row->addLayout(btns);
    root->addLayout(row);

    // Status bar
    m_lblStatus = new QLabel;
    m_lblStatus->setStyleSheet("color: gray; font-size: 10px;");
    root->addWidget(m_lblStatus);

    connect(btnAdd,      &QPushButton::clicked, this, &WireGuardConfigManager::onAdd);
    connect(m_btnEdit,   &QPushButton::clicked, this, &WireGuardConfigManager::onEdit);
    connect(m_btnRemove, &QPushButton::clicked, this, &WireGuardConfigManager::onRemove);
    connect(btnImport,   &QPushButton::clicked, this, &WireGuardConfigManager::onImport);
    connect(m_btnConnect,&QPushButton::clicked, this, &WireGuardConfigManager::onConnect);
    connect(btnClose,    &QPushButton::clicked, this, &QDialog::accept);
    connect(m_list, &QListWidget::currentRowChanged, this, &WireGuardConfigManager::onSelectionChanged);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &WireGuardConfigManager::onEdit);

    // Mirror active tunnel state
    connect(mgr, &WireGuardManager::stateChanged, this, [this](TunnelState s){
        bool active = (s == TunnelState::Connected || s == TunnelState::Connecting);
        m_btnConnect->setEnabled(!active && m_list->currentRow() >= 0);
        if (active)
            m_lblStatus->setText("🟢 Active: " + m_mgr->activeConfig().name);
        else
            m_lblStatus->setText("");
    });

    loadConfigList();
    refreshList();
}

QString WireGuardConfigManager::configDir() const
{
    QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + "/wireguard-configs";
    QDir().mkpath(d);
    return d;
}

void WireGuardConfigManager::loadConfigList()
{
    m_configs.clear();
    QDir dir(configDir());
    for (const QFileInfo& fi : dir.entryInfoList({"*.conf"}, QDir::Files)) {
        QString err;
        WgConfig cfg = WgConfig::fromFile(fi.absoluteFilePath(), err);
        if (!cfg.iface.privateKey.isEmpty())
            m_configs.append(cfg);
    }
}

void WireGuardConfigManager::saveConfigList()
{
    // Each config is persisted as its own .conf file in configDir()
    // (already done by the dialog's save logic; this is a no-op here
    //  unless we want to garbage-collect removed ones)
}

void WireGuardConfigManager::refreshList()
{
    int prev = m_list->currentRow();
    m_list->clear();
    for (const WgConfig& c : m_configs) {
        QString peerInfo = QString(" (%1 peer%2)").arg(c.peers.size()).arg(c.peers.size()!=1?"s":"");
        m_list->addItem(c.name + peerInfo);
    }
    if (prev >= 0 && prev < m_list->count())
        m_list->setCurrentRow(prev);
}

void WireGuardConfigManager::onSelectionChanged()
{
    bool sel = m_list->currentRow() >= 0;
    m_btnEdit->setEnabled(sel);
    m_btnRemove->setEnabled(sel);
    bool active = (m_mgr->state() == TunnelState::Connected ||
                   m_mgr->state() == TunnelState::Connecting);
    m_btnConnect->setEnabled(sel && !active);
}

void WireGuardConfigManager::onAdd()
{
    WireGuardConfigDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    WgConfig cfg = dlg.result();
    cfg.filePath = configDir() + "/" + cfg.name + ".conf";
    QString err;
    if (!cfg.saveToFile(err)) {
        QMessageBox::critical(this, "Save Error", err);
        return;
    }
    m_configs.append(cfg);
    refreshList();
    m_list->setCurrentRow(m_configs.size() - 1);
}

void WireGuardConfigManager::onEdit()
{
    int idx = m_list->currentRow();
    if (idx < 0 || idx >= m_configs.size()) return;

    WireGuardConfigDialog dlg(this, m_configs[idx]);
    if (dlg.exec() != QDialog::Accepted) return;

    WgConfig cfg = dlg.result();
    cfg.filePath = m_configs[idx].filePath;
    // If name changed, update file path
    if (cfg.name != m_configs[idx].name) {
        QString newPath = configDir() + "/" + cfg.name + ".conf";
        QFile::remove(cfg.filePath);
        cfg.filePath = newPath;
    }
    QString err;
    if (!cfg.saveToFile(err)) {
        QMessageBox::critical(this, "Save Error", err);
        return;
    }
    m_configs[idx] = cfg;
    refreshList();
}

void WireGuardConfigManager::onRemove()
{
    int idx = m_list->currentRow();
    if (idx < 0 || idx >= m_configs.size()) return;

    QString name = m_configs[idx].name;
    if (QMessageBox::question(this, "Remove Tunnel",
        "Remove tunnel '" + name + "'?\nThis will delete the config file.",
        QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;

    QFile::remove(m_configs[idx].filePath);
    m_configs.removeAt(idx);
    refreshList();
}

void WireGuardConfigManager::onImport()
{
    QStringList paths = QFileDialog::getOpenFileNames(this,
        "Import WireGuard Configs", {},
        "WireGuard Config (*.conf);;All Files (*)");

    int imported = 0;
    for (const QString& path : paths) {
        QString err;
        WgConfig cfg = WgConfig::fromFile(path, err);
        if (cfg.iface.privateKey.isEmpty()) {
            QMessageBox::warning(this, "Import Error",
                "Failed to import " + QFileInfo(path).fileName() + ":\n" + err);
            continue;
        }
        // Copy to config dir
        cfg.filePath = configDir() + "/" + cfg.name + ".conf";
        if (!cfg.saveToFile(err)) {
            QMessageBox::warning(this, "Save Error", err);
            continue;
        }
        // Avoid duplicates
        bool dup = false;
        for (const auto& existing : m_configs)
            if (existing.name == cfg.name) { dup = true; break; }
        if (!dup) m_configs.append(cfg);
        ++imported;
    }

    if (imported > 0) {
        refreshList();
        QMessageBox::information(this, "Import", QString("Imported %1 tunnel(s).").arg(imported));
    }
}

void WireGuardConfigManager::onConnect()
{
    int idx = m_list->currentRow();
    if (idx < 0 || idx >= m_configs.size()) return;

    m_selectedForConnect = m_configs[idx];
    emit connectRequested(m_configs[idx]);
    accept();
}
