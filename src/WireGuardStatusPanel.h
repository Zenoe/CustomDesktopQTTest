#pragma once
// WireGuardStatusPanel.h
// Dockable / floating panel showing live connection state, stats, and log.

#include "WireGuardManager.h"
#include <QDockWidget>
#include <QLabel>
#include <QTextEdit>
#include <QProgressBar>
#include <QTimer>
#include <QPushButton>
#include <QGroupBox>
#include <QElapsedTimer>

class WireGuardStatusPanel : public QDockWidget
{
    Q_OBJECT
public:
    explicit WireGuardStatusPanel(WireGuardManager* mgr, QWidget* parent = nullptr);

public slots:
    void onStateChanged(TunnelState state);
    void onStatsUpdated(WgStats stats);
    void onLogMessage(const QString& msg, bool isError);

private:
    void buildUI();
    void updateUptime();
    static QString formatBytes(quint64 bytes);
    static QString stateText(TunnelState s);
    static QString stateStyle(TunnelState s);

    void emit_log_internal(const QString& msg);
    WireGuardManager* m_mgr;

    // State group
    QLabel*   m_lblState      = nullptr;
    QLabel*   m_lblTunnel     = nullptr;
    QLabel*   m_lblUptime     = nullptr;
    QLabel*   m_lblLastHS     = nullptr;

    // Stats
    QLabel*   m_lblRx         = nullptr;
    QLabel*   m_lblTx         = nullptr;
    QLabel*   m_lblEndpoint   = nullptr;

    // Log
    QTextEdit* m_log          = nullptr;

    QPushButton* m_btnConnect    = nullptr;
    QPushButton* m_btnDisconnect = nullptr;

    QTimer*      m_uptimeTimer   = nullptr;
    QElapsedTimer m_connectedAt;
    bool         m_connected     = false;
};
