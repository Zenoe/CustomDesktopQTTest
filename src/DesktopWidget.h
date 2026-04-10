#include <QListWidget>
#include <QMap>
#include "boxfilemanager.h"

class QListWidgetItem;
class QContextMenuEvent;
class QDropEvent;
class QDragEnterEvent;
class QDragMoveEvent;

class DesktopWidget : public QListWidget
{
    Q_OBJECT
public:
    explicit DesktopWidget(QWidget* parent = nullptr);

    // Call on startup — pass path to your .box file
    void loadFromBoxFile(const QString& filePath);

    // Call on close / as needed
    void saveToBoxFile(const QString& filePath);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event)  override;
    void dropEvent(QDropEvent* event)  override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void launchItem(QListWidgetItem* item);
    void openItem();
    void removeItem();
    void showProperties();

private:
    // Converts a grid (row,col) back to a pixel position
    QPoint gridToPos(int row, int col) const;

    // Reads the item's current visual position → grid cell
    void   posToGrid(QListWidgetItem* item, int& row, int& col) const;

    QListWidgetItem* contextItem = nullptr;
    QMap<QString, QString> m_contents;  // contentKey → text body
    int m_contentCounter = 0;           // for generating unique content keys
};