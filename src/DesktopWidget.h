#pragma once
#include <QListWidget>
#include <QContextMenuEvent>

class DesktopWidget : public QListWidget
{
    Q_OBJECT
public:
    DesktopWidget(QWidget* parent = nullptr);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void launchItem(QListWidgetItem* item);
    void openItem();
    void removeItem();
    void showProperties();

private:
    QListWidgetItem* contextItem = nullptr;


};