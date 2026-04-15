// WireGuardStatusPanel.cpp
#include "WireGuardStatusPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollBar>
#include <QDateTime>
#include <QFont>

WireGuardStatusPanel::WireGuardStatusPanel(WireGuardManager* mgr, QWidget* parent)
    : QDockWidget("WireGuard Status", parent), m_mgr(mgr)
{
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(QDockWidget::DockWidgetMovable |
                QDockWidget::DockWidgetFloatable |
                QDockWidget::DockWidgetClosable);
    buildUI();

    connect(mgr, &WireGuardManager::stateChanged,  this, &WireGuardStatusPanel::onStateChanged);
    connect(mgr, &WireGuardManager::statsUpdated,  this, &WireGuardStatusPanel::onStatsUpdated);
    connect(mgr, &WireGuardManager::logMessage,    this, &WireGuardStatusPanel::onLogMessage);
    connect(mgr, &WireGuardManager::errorOccurred, this, [this](const QString& e){
        onLogMessage("ERROR: " + e, true);
    });

    m_uptimeTimer = new QTimer(this);
    m_uptimeTimer->setInterval(1000);
    connect(m_uptimeTimer, &QTimer::timeout, this, &WireGuardStatusPanel::updateUptime);

    onStateChanged(TunnelState::Disconnected);
}

void WireGuardStatusPanel::buildUI()
{
    auto* root = new QWidget;
    auto* vbox = new QVBoxLayout(root);
    vbox->setSpacing(8);

    // ── Status indicator ─────────────────────────────────────────────────────
    auto* stateGroup = new QGroupBox("Connection");
    auto* stateForm  = new QFormLayout(stateGroup);

    m_lblState   = new QLabel("Disconnected");
    m_lblState->setStyleSheet("font-weight: bold; font-size: 14px; padding: 4px;");
    m_lblTunnel  = new QLabel("—");
    m_lblUptime  = new QLabel("—");
    m_lblLastHS  = new QLabel("—");

    stateForm->addRow("Status:",         m_lblState);
    stateForm->addRow("Tunnel:",         m_lblTunnel);
    stateForm->addRow("Uptime:",         m_lblUptime);
    stateForm->addRow("Last Handshake:", m_lblLastHS);
    vbox->addWidget(stateGroup);

    // ── Traffic stats ────────────────────────────────────────────────────────
    auto* statsGroup = new QGroupBox("Traffic");
    auto* statsForm  = new QFormLayout(statsGroup);
    m_lblRx       = new QLabel("0 B");
    m_lblTx       = new QLabel("0 B");
    m_lblEndpoint = new QLabel("—");
    statsForm->addRow("↓ Received:", m_lblRx);
    statsForm->addRow("↑ Sent:",     m_lblTx);
    statsForm->addRow("Endpoint:",   m_lblEndpoint);
    vbox->addWidget(statsGroup);

    // ── Quick actions ────────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout;
    m_btnConnect    = new QPushButton("⚡ Connect");
    m_btnDisconnect = new QPushButton("⏹ Disconnect");
    m_btnDisconnect->setEnabled(false);
    btnRow->addWidget(m_btnConnect);
    btnRow->addWidget(m_btnDisconnect);
    vbox->addLayout(btnRow);

    connect(m_btnConnect,    &QPushButton::clicked, this, [this](){
        // Emit a signal that MainWindow catches to open connect dialog
        // (avoids circular dependency)
        onLogMessage("Use VPN menu → Connect to initiate connection", false);
    });
    connect(m_btnDisconnect, &QPushButton::clicked, this, [this](){
        m_mgr->disconnectTunnel();
    });

    // ── Log ──────────────────────────────────────────────────────────────────
    auto* logGroup = new QGroupBox("Log");
    auto* logBox   = new QVBoxLayout(logGroup);
    m_log = new QTextEdit;
    m_log->setReadOnly(true);
    m_log->document()->setMaximumBlockCount(500);
    QFont f("Consolas", 8);
    m_log->setFont(f);
    m_log->setMaximumHeight(160);
    logBox->addWidget(m_log);
    auto* btnClearLog = new QPushButton("Clear Log");
    btnClearLog->setMaximumWidth(80);
    logBox->addWidget(btnClearLog, 0, Qt::AlignRight);
    connect(btnClearLog, &QPushButton::clicked, m_log, &QTextEdit::clear);
    vbox->addWidget(logGroup);

    vbox->addStretch();
    setWidget(root);
}

// ── Slots ─────────────────────────────────────────────────────────────────────
void WireGuardStatusPanel::onStateChanged(TunnelState state)
{
    m_lblState->setText(stateText(state));
    m_lblState->setStyleSheet(stateStyle(state) +
        " font-weight: bold; font-size: 14px; padding: 4px; border-radius: 4px;");

    bool connected = (state == TunnelState::Connected);
    bool busy      = (state == TunnelState::Connecting || state == TunnelState::Disconnecting);

    m_btnConnect->setEnabled(!connected && !busy);
    m_btnDisconnect->setEnabled(connected || busy);

    if (connected && !m_connected) {
        m_connected = true;
        m_connectedAt.restart();
        m_uptimeTimer->start();
        m_lblTunnel->setText(m_mgr->activeConfig().name);
        onLogMessage("Tunnel connected: " + m_mgr->activeConfig().name, false);
    } else if (!connected && m_connected) {
        m_connected = false;
        m_uptimeTimer->stop();
        m_lblUptime->setText("—");
        m_lblTunnel->setText("—");
        m_lblLastHS->setText("—");
        m_lblRx->setText("0 B");
        m_lblTx->setText("0 B");
    }
}

void WireGuardStatusPanel::onStatsUpdated(WgStats stats)
{
    m_lblRx->setText(formatBytes(stats.rxBytes));
    m_lblTx->setText(formatBytes(stats.txBytes));

    if (stats.lastHandshake.isValid()) {
        qint64 secsAgo = stats.lastHandshake.secsTo(QDateTime::currentDateTimeUtc());
        if (secsAgo < 3600)
            m_lblLastHS->setText(QString("%1 s ago").arg(secsAgo));
        else
            m_lblLastHS->setText(stats.lastHandshake.toLocalTime().toString("hh:mm:ss"));
    }

    if (!m_mgr->activeConfig().peers.isEmpty())
        m_lblEndpoint->setText(m_mgr->activeConfig().peers.first().endpoint);
}

void WireGuardStatusPanel::onLogMessage(const QString& msg, bool isError)
{
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString html = QString("<span style='color:%1'>[%2] %3</span>")
        .arg(isError ? "#e74c3c" : "#bdc3c7", ts, msg.toHtmlEscaped());
    m_log->append(html);
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

// ── Helpers ───────────────────────────────────────────────────────────────────
void WireGuardStatusPanel::updateUptime()
{
    if (!m_connected) return;
    qint64 s = m_connectedAt.elapsed() / 1000;
    QString uptime = QString("%1:%2:%3")
        .arg(s/3600, 2, 10, QChar('0'))
        .arg((s%3600)/60, 2, 10, QChar('0'))
        .arg(s%60, 2, 10, QChar('0'));
    m_lblUptime->setText(uptime);
}

QString WireGuardStatusPanel::formatBytes(quint64 bytes)
{
    if (bytes < 1024)         return QString("%1 B").arg(bytes);
    if (bytes < 1024*1024)    return QString("%1 KiB").arg(bytes/1024.0, 0,'f',1);
    if (bytes < 1024*1024*1024) return QString("%1 MiB").arg(bytes/(1024.0*1024),0,'f',2);
    return QString("%1 GiB").arg(bytes/(1024.0*1024*1024),0,'f',2);
}

QString WireGuardStatusPanel::stateText(TunnelState s)
{
    switch(s) {
        case TunnelState::Disconnected:  return "Disconnected";
        case TunnelState::Connecting:    return "Connecting…";
        case TunnelState::Connected:     return "Connected ✔";
        case TunnelState::Disconnecting: return "Disconnecting…";
        case TunnelState::Error:         return "Error ✖";
    }
    return "Unknown";
}

QString WireGuardStatusPanel::stateStyle(TunnelState s)
{
    switch(s) {
        case TunnelState::Connected:     return "background:#27ae60; color:white;";
        case TunnelState::Connecting:    return "background:#f39c12; color:white;";
        case TunnelState::Disconnecting: return "background:#e67e22; color:white;";
        case TunnelState::Error:         return "background:#e74c3c; color:white;";
        default:                         return "background:#7f8c8d; color:white;";
    }
}

void WireGuardStatusPanel::emit_log_internal(const QString& msg)
{
    onLogMessage(msg, false);
}
