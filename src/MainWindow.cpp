// MainWindow.cpp
#include "MainWindow.h"
#include "DesktopWidget.h"
#include "WireGuardConfigManager.h"
#include "WireGuardConfigDialog.h"
#include <QStandardPaths>
#include <QCloseEvent>
#include <QMessageBox>
#include <QStatusBar>
#include <QMenuBar>
#include <QLabel>
#include <QApplication>

static const QString BOX_FILE =
QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
+ "/desktop.box";

// ── Ctor ──────────────────────────────────────────────────────────────────────
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    resize(900, 600);
    setWindowTitle("Custom Desktop Box");

    // ── WireGuard manager (create before menu) ────────────────────────────────
    m_wgMgr = new WireGuardManager(this);
    connect(m_wgMgr, &WireGuardManager::stateChanged,
        this, &MainWindow::onTunnelStateChanged);
    connect(m_wgMgr, &WireGuardManager::errorOccurred,
        this, &MainWindow::onTunnelError);

    // ── Status panel (dockable) ───────────────────────────────────────────────
    m_statusPanel = new WireGuardStatusPanel(m_wgMgr, this);
    addDockWidget(Qt::RightDockWidgetArea, m_statusPanel);
    m_statusPanel->hide();   // hidden by default; user opens via menu

    // ── Central widget ────────────────────────────────────────────────────────
    desktop = new DesktopWidget(this);
    setCentralWidget(desktop);
    desktop->loadFromBoxFile(BOX_FILE);

    // ── Menu + tray ───────────────────────────────────────────────────────────
    buildMenuBar();
    buildTrayIcon();
    statusBar()->showMessage("Ready");
}

MainWindow::~MainWindow()
{
    desktop->saveToBoxFile(BOX_FILE);
    // Ensure tunnel is stopped on exit
    if (m_wgMgr->state() != TunnelState::Disconnected)
        m_wgMgr->disconnectTunnel();
}

// ── Menu bar ──────────────────────────────────────────────────────────────────
void MainWindow::buildMenuBar()
{
    // File menu (existing)
    QMenu* fileMenu = menuBar()->addMenu("&File");
    QAction* exitAction = fileMenu->addAction("E&xit");
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);

    // VPN menu (new)
    QMenu* vpnMenu = menuBar()->addMenu("&VPN");

    QAction* actManage = vpnMenu->addAction(QIcon::fromTheme("network-vpn"),
        "&Manage Tunnels…");
    actManage->setShortcut(QKeySequence("Ctrl+Shift+W"));
    connect(actManage, &QAction::triggered, this, &MainWindow::onVpnManageTunnels);

    vpnMenu->addSeparator();

    m_actConnect = vpnMenu->addAction("&Connect…");
    m_actConnect->setShortcut(QKeySequence("Ctrl+Shift+C"));
    connect(m_actConnect, &QAction::triggered, this, &MainWindow::onVpnConnect);

    m_actDisconnect = vpnMenu->addAction("&Disconnect");
    m_actDisconnect->setShortcut(QKeySequence("Ctrl+Shift+D"));
    m_actDisconnect->setEnabled(false);
    connect(m_actDisconnect, &QAction::triggered, this, &MainWindow::onVpnDisconnect);

    vpnMenu->addSeparator();

    m_actStatus = vpnMenu->addAction("Show &Status Panel");
    m_actStatus->setCheckable(true);
    m_actStatus->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(m_actStatus, &QAction::triggered, this, &MainWindow::onVpnToggleStatusPanel);
    connect(m_statusPanel, &QDockWidget::visibilityChanged, m_actStatus, &QAction::setChecked);
}

// ── System tray ───────────────────────────────────────────────────────────────
void MainWindow::buildTrayIcon()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;

    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(QIcon::fromTheme("network-vpn",
        QIcon(":/icons/wireguard.png")));  // embed or provide icon
    m_tray->setToolTip("Custom Desktop Box – VPN: Disconnected");

    m_trayMenu = new QMenu;

    QAction* actRestore = m_trayMenu->addAction("Show Window");
    connect(actRestore, &QAction::triggered, this, &QMainWindow::showNormal);

    m_trayMenu->addSeparator();

    m_trayActConnect = m_trayMenu->addAction("Connect VPN…");
    connect(m_trayActConnect, &QAction::triggered, this, &MainWindow::onVpnConnect);

    m_trayActDisconnect = m_trayMenu->addAction("Disconnect VPN");
    m_trayActDisconnect->setEnabled(false);
    connect(m_trayActDisconnect, &QAction::triggered, this, &MainWindow::onVpnDisconnect);

    m_trayMenu->addSeparator();

    QAction* actQuit = m_trayMenu->addAction("Quit");
    connect(actQuit, &QAction::triggered, qApp, &QApplication::quit);

    m_tray->setContextMenu(m_trayMenu);
    connect(m_tray, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);
    m_tray->show();
}

// ── VPN menu slots ────────────────────────────────────────────────────────────
void MainWindow::onVpnManageTunnels()
{
    WireGuardConfigManager mgr(m_wgMgr, this);
    connect(&mgr, &WireGuardConfigManager::connectRequested,
        m_wgMgr, &WireGuardManager::connectTunnel);
    mgr.exec();
}

void MainWindow::onVpnConnect()
{
    // Open the tunnel manager; user picks a tunnel and clicks Connect
    WireGuardConfigManager mgr(m_wgMgr, this);
    connect(&mgr, &WireGuardConfigManager::connectRequested,
        this, [this](const WgConfig& cfg) {
            m_wgMgr->connectTunnel(cfg);
            m_statusPanel->show();
            m_actStatus->setChecked(true);
        });
    mgr.exec();
}

void MainWindow::onVpnDisconnect()
{
    if (QMessageBox::question(this, "Disconnect VPN",
        "Disconnect from '" + m_wgMgr->activeConfig().name + "'?",
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
    {
        m_wgMgr->disconnectTunnel();
    }
}

void MainWindow::onVpnToggleStatusPanel()
{
    m_statusPanel->setVisible(!m_statusPanel->isVisible());
}

// ── Manager signal handlers ───────────────────────────────────────────────────
void MainWindow::onTunnelStateChanged(TunnelState state)
{
    updateVpnMenuState(state);

    // Update window title and tray tooltip
    QString tunnelName = m_wgMgr->activeConfig().name;
    switch (state) {
    case TunnelState::Connected:
        setWindowTitle("Custom Desktop Box  [VPN: " + tunnelName + " ✔]");
        m_tray->setToolTip("VPN: Connected – " + tunnelName);
        m_tray->showMessage("VPN Connected",
            "Tunnel '" + tunnelName + "' is active.",
            QSystemTrayIcon::Information, 3000);
        break;
    case TunnelState::Disconnected:
        setWindowTitle("Custom Desktop Box");
        m_tray->setToolTip("VPN: Disconnected");
        break;
    case TunnelState::Error:
        setWindowTitle("Custom Desktop Box  [VPN: Error]");
        m_tray->setToolTip("VPN: Error");
        break;
    default: break;
    }

    // Status bar indicator
    static const QMap<TunnelState, QString> stateStr{
        {TunnelState::Disconnected,  "VPN: Disconnected"},
        {TunnelState::Connecting,    "VPN: Connecting…"},
        {TunnelState::Connected,     "VPN: Connected"},
        {TunnelState::Disconnecting, "VPN: Disconnecting…"},
        {TunnelState::Error,         "VPN: Error"},
    };
    statusBar()->showMessage(stateStr.value(state, "VPN: Unknown"));
}

void MainWindow::onTunnelError(const QString& msg)
{
    m_statusPanel->show();
    m_actStatus->setChecked(true);
    m_tray->showMessage("VPN Error", msg, QSystemTrayIcon::Critical, 5000);
}

// ── Helpers ───────────────────────────────────────────────────────────────────
void MainWindow::updateVpnMenuState(TunnelState state)
{
    bool connected = (state == TunnelState::Connected);
    bool busy = (state == TunnelState::Connecting ||
        state == TunnelState::Disconnecting);

    m_actConnect->setEnabled(!connected && !busy);
    m_actDisconnect->setEnabled(connected || busy);
    if (m_trayActConnect)    m_trayActConnect->setEnabled(!connected && !busy);
    if (m_trayActDisconnect) m_trayActDisconnect->setEnabled(connected || busy);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_tray && m_tray->isVisible() &&
        m_wgMgr->state() == TunnelState::Connected)
    {
        hide();
        m_tray->showMessage("Running in background",
            "VPN is still active. Right-click the tray icon to quit.",
            QSystemTrayIcon::Information, 3000);
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick)
        showNormal();
}
