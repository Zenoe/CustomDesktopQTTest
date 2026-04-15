#pragma once
// MainWindow.h  –  with full WireGuard integration

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>

#include "WireGuardManager.h"
#include "WireGuardStatusPanel.h"

class DesktopWidget;  // your existing widget

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    // VPN menu actions
    void onVpnManageTunnels();
    void onVpnConnect();
    void onVpnDisconnect();
    void onVpnToggleStatusPanel();

    // Manager signals
    void onTunnelStateChanged(TunnelState state);
    void onTunnelError(const QString& msg);

    // Tray
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void buildMenuBar();
    void buildTrayIcon();
    void updateVpnMenuState(TunnelState state);

    DesktopWidget* desktop = nullptr;
    WireGuardManager* m_wgMgr = nullptr;
    WireGuardStatusPanel* m_statusPanel = nullptr;

    // VPN menu actions (kept for enable/disable updates)
    QAction* m_actConnect = nullptr;
    QAction* m_actDisconnect = nullptr;
    QAction* m_actStatus = nullptr;

    // System tray
    QSystemTrayIcon* m_tray = nullptr;
    QMenu* m_trayMenu = nullptr;
    QAction* m_trayActConnect = nullptr;
    QAction* m_trayActDisconnect = nullptr;
};
