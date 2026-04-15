#pragma once
// WireGuardConfigDialog.h
// Full-featured config editor: import .conf, edit all fields, manage peers,
// generate keypairs via tunnel.dll.

#include "WireGuardConfig.h"
#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QSpinBox>
#include <QTabWidget>
#include <QPushButton>
#include <QLabel>

class PeerWidget;

class WireGuardConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit WireGuardConfigDialog(QWidget* parent = nullptr,
                                   const WgConfig& existing = {});

    WgConfig result() const { return m_config; }

private slots:
    void onImportConf();
    void onExportConf();
    void onGenerateKeypair();
    void onAddPeer();
    void onRemovePeer();
    void onPeerSelected(int index);
    void onAccept();

private:
    void buildUI();
    void populateFromConfig(const WgConfig& cfg);
    WgConfig collectConfig() const;
    void updatePeerList();

    // Interface tab widgets
    QLineEdit* m_editName         = nullptr;
    QLineEdit* m_editPrivKey      = nullptr;
    QLabel*    m_lblPubKey        = nullptr;
    QLineEdit* m_editAddresses    = nullptr;
    QSpinBox*  m_spinPort         = nullptr;
    QLineEdit* m_editDNS          = nullptr;
    QLineEdit* m_editMTU          = nullptr;

    // Peer list + editor
    QListWidget*  m_peerList     = nullptr;
    QLineEdit*    m_peerName     = nullptr;
    QLineEdit*    m_peerPubKey   = nullptr;
    QLineEdit*    m_peerPSK      = nullptr;
    QLineEdit*    m_peerEndpoint = nullptr;
    QLineEdit*    m_peerAllowedIPs = nullptr;
    QSpinBox*     m_peerKeepalive = nullptr;
    QPushButton*  m_btnRemPeer   = nullptr;

    // Raw conf view
    QTextEdit*   m_rawEdit       = nullptr;

    WgConfig m_config;
    int      m_currentPeerIdx = -1;
};
