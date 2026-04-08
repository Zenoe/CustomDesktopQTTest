#include "DesktopWidget.h"
#include <QDragEnterEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QProcess>
#include <QIcon>
#include <QFileIconProvider>
#include <QMenu>
#include <QAction>
#include <QCursor>
#include <QMessageBox>
#include <windows.h>
#include <shellapi.h>

#include <QImage>
#include <QPixmap>
// return null icon
QIcon getFileIcon(const QString& filePath)
{
    SHFILEINFO sfi = {};
    if (SHGetFileInfo(reinterpret_cast<const wchar_t*>(filePath.utf16()),
        0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON)) {

        QIcon icon = QIcon(QPixmap::fromImage(QImage::fromHICON(sfi.hIcon)));

        if (sfi.hIcon) {
            DestroyIcon(sfi.hIcon);
        }

        return icon;
    }
    return QIcon();
}

DesktopWidget::DesktopWidget(QWidget* parent)
    : QListWidget(parent)
{
    setViewMode(QListView::IconMode);
    setIconSize(QSize(48,48));
    setAcceptDrops(true);

    setDragEnabled(true);
    setDropIndicatorShown(true);
    setDefaultDropAction(Qt::MoveAction);

    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setMovement(QListView::Snap);
    setResizeMode(QListView::Adjust);

    setDragDropMode(QAbstractItemView::InternalMove);

    //setDragDropMode(QAbstractItemView::DragDrop);
    setSelectionRectVisible(true);

    setGridSize(QSize(100, 100));
    setSpacing(10);

    connect(this, &QListWidget::itemDoubleClicked,
            this, &DesktopWidget::launchItem);
}

void DesktopWidget::dragEnterEvent(QDragEnterEvent* event)
{
    // Accept both internal item moves and external file drops
    if (event->source() == this ||
        event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
    }
}

void DesktopWidget::dragMoveEvent(QDragMoveEvent* event)
{
    event->acceptProposedAction();
}

void DesktopWidget::dropEvent(QDropEvent* event)
{
    // ── Internal move: let Qt's built-in machinery handle it ──────────────
    if (event->source() == this)
    {
        QListWidget::dropEvent(event);   // moves the item to the new grid cell
        return;
    }

    // ── External drop: files dragged in from Explorer ─────────────────────
    const auto urls = event->mimeData()->urls();
    if (urls.isEmpty())
    {
        event->ignore();
        return;
    }

    QFileIconProvider iconProvider;
    for (const QUrl& url : urls)
    {
        QString path = url.toLocalFile();
        QFileInfo info(path);
        if (!info.exists())
            continue;

        QListWidgetItem* item = new QListWidgetItem();
        item->setFlags(item->flags() | Qt::ItemIsDragEnabled
            | Qt::ItemIsSelectable
            | Qt::ItemIsEnabled);
        item->setText(info.baseName());
        item->setData(Qt::UserRole, path);
        item->setIcon(iconProvider.icon(info));
        addItem(item);
    }
    event->acceptProposedAction();
}

//void DesktopWidget::dropEvent(QDropEvent* event)
//{
//    const auto urls = event->mimeData()->urls();
//
//    QFileIconProvider iconProvider;
//
//    for (const QUrl& url : urls)
//    {
//        QString path = url.toLocalFile();
//        QFileInfo info(path);
//
//        if (!info.exists())
//            continue;
//
//        QListWidgetItem* item = new QListWidgetItem();
//
//        item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
//        item->setText(info.baseName());
//        item->setData(Qt::UserRole, path);
//
//        // ⭐ REAL WINDOWS ICON
//        QIcon icon = iconProvider.icon(info);
//        //item->setIcon(getFileIcon(path));
//        item->setIcon(icon);
//
//        addItem(item);
//    }
//
//    event->acceptProposedAction();
//}


void DesktopWidget::launchItem(QListWidgetItem* item)
{
    QString path = item->data(Qt::UserRole).toString();
    QProcess::startDetached(path);
}

void DesktopWidget::contextMenuEvent(QContextMenuEvent* event)
{
    contextItem = itemAt(event->pos());

    if (!contextItem)
        return;

    QMenu menu(this);

    QAction* openAct =
        menu.addAction(QIcon(":/icons/open.png"), "Open");

    QAction* removeAct =
        menu.addAction(QIcon(":/icons/delete.png"), "Remove");
    //QIcon saveIcon(":/icons/resources/icons/save.png");
    QAction* propAct =
        menu.addAction(QIcon(":/icons/info.png"), "Properties");

    connect(openAct, &QAction::triggered,
        this, &DesktopWidget::openItem);

    connect(removeAct, &QAction::triggered,
        this, &DesktopWidget::removeItem);

    connect(propAct, &QAction::triggered,
        this, &DesktopWidget::showProperties);

    menu.exec(event->globalPos());
}

void DesktopWidget::openItem()
{
    if (!contextItem) return;

    QString path =
        contextItem->data(Qt::UserRole).toString();

    QProcess::startDetached(path);
}

void DesktopWidget::removeItem()
{
    if (!contextItem) return;

    delete takeItem(row(contextItem));
    contextItem = nullptr;
}

void DesktopWidget::showProperties()
{
    if (!contextItem) return;

    QString path =
        contextItem->data(Qt::UserRole).toString();

    QMessageBox::information(
        this,
        "Properties",
        "Path:\n" + path
    );
}
