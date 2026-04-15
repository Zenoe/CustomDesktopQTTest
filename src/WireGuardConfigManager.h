#pragma once
// WireGuardConfigManager.h
// Manages the list of saved WireGuard configs (like the official client's tunnel list).

#include "WireGuardConfig.h"
#include "WireGuardManager.h"
#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>

class WireGuardConfigManager : public QDialog
{
    Q_OBJECT
public:
    explicit WireGuardConfigManager(WireGuardManager* mgr, QWidget* parent = nullptr);

    // Returns the config chosen for connection (if any)
    WgConfig selectedForConnect() const { return m_selectedForConnect; }

signals:
    void connectRequested(WgConfig cfg);

private slots:
    void onAdd();
    void onEdit();
    void onRemove();
    void onImport();
    void onConnect();
    void onSelectionChanged();
    void refreshList();

private:
    void saveConfigList();
    void loadConfigList();
    QString configDir() const;

    WireGuardManager* m_mgr;
    QListWidget*  m_list      = nullptr;
    QPushButton*  m_btnEdit   = nullptr;
    QPushButton*  m_btnRemove = nullptr;
    QPushButton*  m_btnConnect= nullptr;
    QLabel*       m_lblStatus = nullptr;

    QList<WgConfig> m_configs;
    WgConfig        m_selectedForConnect;
};
