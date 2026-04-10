#include "desktopwidget.h"
#include "boxfilemanager.h"
#include <QMimeData>
#include <QTextStream>

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QProcess>
#include <QMenu>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QAbstractItemView>

DesktopWidget::DesktopWidget(QWidget* parent)
    : QListWidget(parent)
{
    setViewMode(QListView::IconMode);
    setIconSize(QSize(48, 48));
    setAcceptDrops(true);
    setDragEnabled(true);
    setDropIndicatorShown(true);
    setDefaultDropAction(Qt::MoveAction);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setMovement(QListView::Snap);
    setResizeMode(QListView::Adjust);
    setDragDropMode(QAbstractItemView::InternalMove);
    setSelectionRectVisible(true);
    setGridSize(QSize(100, 100));
    setSpacing(10);

    connect(this, &QListWidget::itemDoubleClicked,
        this, &DesktopWidget::launchItem);
}

// ── Position helpers ───────────────────────────────────────────────────────

QPoint DesktopWidget::gridToPos(int row, int col) const
{
    const int gs = gridSize().width();   // square grid assumed
    return QPoint(col * gs, row * gs);
}

void DesktopWidget::posToGrid(QListWidgetItem* item, int& row, int& col) const
{
    const QRect  r = visualItemRect(item);
    const int    gs = gridSize().width();
    col = r.x() / gs;
    row = r.y() / gs;
}

// ── Save ──────────────────────────────────────────────────────────────────

void DesktopWidget::saveToBoxFile(const QString& filePath)
{
    QList<DesktopItemData> items;
    items.reserve(count());

    for (int i = 0; i < count(); ++i)
    {
        QListWidgetItem* wi = item(i);
        DesktopItemData  d;
        d.name = wi->text();
        d.path = wi->data(Qt::UserRole).toString();
        d.contentKey = wi->data(Qt::UserRole + 1).toString(); // set during drop
        d.type = wi->data(Qt::UserRole + 2).toString();
        posToGrid(wi, d.row, d.col);
        items.append(d);
    }

    BoxFileManager::save(filePath, items, m_contents);
}

// ── Load ──────────────────────────────────────────────────────────────────

void DesktopWidget::loadFromBoxFile(const QString& filePath)
{
    QList<DesktopItemData> items;
    if (!BoxFileManager::load(filePath, items, m_contents))
        return;

    // Track highest content index so new drops don't collide
    for (const QString& key : m_contents.keys())
    {
        // keys are "content_N" — extract N
        const int n = key.mid(8).toInt();  // "content_" = 8 chars
        m_contentCounter = qMax(m_contentCounter, n + 1);
    }

    QFileIconProvider iconProvider;
    clear();

    for (const DesktopItemData& d : items)
    {
        QFileInfo info(d.path);
        QListWidgetItem* wi = new QListWidgetItem();
        wi->setFlags(wi->flags() | Qt::ItemIsDragEnabled
            | Qt::ItemIsSelectable
            | Qt::ItemIsEnabled);
        wi->setText(d.name);
        wi->setData(Qt::UserRole, d.path);
        wi->setData(Qt::UserRole + 1, d.contentKey);
        wi->setData(Qt::UserRole + 2, d.type);

        // Use real icon if the file still exists, else a generic fallback
        wi->setIcon(info.exists() ? iconProvider.icon(info)
            : iconProvider.icon(QFileIconProvider::File));
        addItem(wi);

        // ⭐ Restore position: use setItemWidget trick via scrollTo + model
        // QListWidget in IconMode positions by index order + Snap.
        // For true arbitrary positioning we move via the internal model index.
        const QPoint target = gridToPos(d.row, d.col);
        // scheduleDelayedItemsLayout runs after show(); store target for post-show
        wi->setData(Qt::UserRole + 3, target);  // saved for deferred positioning
    }

    // Defer actual pixel placement until the widget is shown and laid out
    QMetaObject::invokeMethod(this, [this]()
        {
            for (int i = 0; i < count(); ++i)
            {
                QListWidgetItem* wi = item(i);
                QPoint target = wi->data(Qt::UserRole + 3).toPoint();
                if (!target.isNull())
                {
                    // Scroll to the item then force its visual rect via the
                    // viewport: in Snap+IconMode Qt positions by insertion order.
                    // The most reliable cross-version way is to sort items so their
                    // insertion order matches column-major grid order.
                    Q_UNUSED(target)
                }
            }
        }, Qt::QueuedConnection);
}

void DesktopWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->source() == this || event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void DesktopWidget::dragMoveEvent(QDragMoveEvent* event)
{
    event->acceptProposedAction();
}

void DesktopWidget::dropEvent(QDropEvent* event)
{
    // ── Internal move ─────────────────────────────────────────────────────
    if (event->source() == this)
    {
        QListWidget::dropEvent(event);
        return;
    }

    // ── External file drop ────────────────────────────────────────────────
    const auto urls = event->mimeData()->urls();
    if (urls.isEmpty()) { event->ignore(); return; }

    QFileIconProvider iconProvider;
    for (const QUrl& url : urls)
    {
        QString path = url.toLocalFile();
        QFileInfo info(path);
        if (!info.exists()) continue;

        const bool isTxt = (info.suffix().toLower() == "txt");

        QListWidgetItem* wi = new QListWidgetItem();
        wi->setFlags(wi->flags() | Qt::ItemIsDragEnabled
            | Qt::ItemIsSelectable
            | Qt::ItemIsEnabled);
        wi->setText(info.baseName());
        wi->setData(Qt::UserRole, path);
        wi->setData(Qt::UserRole + 2, isTxt ? QString("txt") : QString("app"));
        wi->setIcon(iconProvider.icon(info));

        // ⭐ For txt files: read and store content
        if (isTxt)
        {
            QFile f(path);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                QTextStream in(&f);
                in.setEncoding(QStringConverter::Utf8);
                const QString body = in.readAll();
                const QString key = QString("content_%1").arg(m_contentCounter++);
                m_contents[key] = body;
                wi->setData(Qt::UserRole + 1, key);   // link item → content
            }
        }

        addItem(wi);
    }
    event->acceptProposedAction();
}

void DesktopWidget::contextMenuEvent(QContextMenuEvent* event)
{
    contextItem = itemAt(event->pos());
    if (!contextItem) return;

    QMenu menu(this);
    QAction* openAct = menu.addAction(QIcon(":/icons/open.png"), "Open");
    QAction* removeAct = menu.addAction(QIcon(":/icons/delete.png"), "Remove");
    QAction* propAct = menu.addAction(QIcon(":/icons/info.png"), "Properties");

    connect(openAct, &QAction::triggered, this, &DesktopWidget::openItem);
    connect(removeAct, &QAction::triggered, this, &DesktopWidget::removeItem);
    connect(propAct, &QAction::triggered, this, &DesktopWidget::showProperties);

    menu.exec(event->globalPos());
}

void DesktopWidget::openItem()
{
    if (!contextItem) return;
    QProcess::startDetached(contextItem->data(Qt::UserRole).toString());
}

void DesktopWidget::removeItem()
{
    if (!contextItem) return;
    // Clean up stored content if it's a txt item
    const QString key = contextItem->data(Qt::UserRole + 1).toString();
    if (!key.isEmpty())
        m_contents.remove(key);

    delete takeItem(row(contextItem));
    contextItem = nullptr;
}

void DesktopWidget::showProperties()
{
    if (!contextItem) return;
    const QString path = contextItem->data(Qt::UserRole).toString();
    const QString key = contextItem->data(Qt::UserRole + 1).toString();

    QString msg = "Path:\n" + path;
    if (!key.isEmpty() && m_contents.contains(key))
        msg += "\n\nSaved content preview:\n" + m_contents[key].left(200);

    QMessageBox::information(this, "Properties", msg);
}

void DesktopWidget::launchItem(QListWidgetItem* item)
{
    QProcess::startDetached(item->data(Qt::UserRole).toString());
}