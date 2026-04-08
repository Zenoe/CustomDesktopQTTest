#pragma once
#include <QString>
#include <QList>

struct DesktopItemData
{
    QString name;        // display name
    QString path;        // original file path
    int     row = 0;   // grid row  (pos.y / gridSize)
    int     col = 0;   // grid col  (pos.x / gridSize)
    QString type;        // "app" | "txt" | "file"
    QString contentKey;  // non-empty only for txt items
};

class BoxFileManager
{
public:
    static bool save(const QString& filePath,
        const QList<DesktopItemData>& items,
        const QMap<QString, QString>& contents);

    static bool load(const QString& filePath,
        QList<DesktopItemData>& items,
        QMap<QString, QString>& contents);
};