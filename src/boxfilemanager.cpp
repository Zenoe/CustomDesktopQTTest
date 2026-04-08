#include "boxfilemanager.h"
#include <QSettings>
#include <QFileInfo>

bool BoxFileManager::save(const QString& filePath,
    const QList<DesktopItemData>& items,
    const QMap<QString, QString>& contents)
{
    QSettings s(filePath, QSettings::IniFormat);
    s.setIniCodec("UTF-8");   // handles paths with unicode chars

    s.beginGroup("BOXFILE");
    s.setValue("version", 1);
    s.endGroup();

    // ©¤©¤ LAYOUT ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤
    s.beginGroup("LAYOUT");
    s.setValue("count", items.size());

    for (int i = 0; i < items.size(); ++i)
    {
        const DesktopItemData& d = items[i];
        const QString pfx = QString("item%1").arg(i);
        s.setValue(pfx + ".name", d.name);
        s.setValue(pfx + ".path", d.path);
        s.setValue(pfx + ".row", d.row);
        s.setValue(pfx + ".col", d.col);
        s.setValue(pfx + ".type", d.type);
        s.setValue(pfx + ".contentKey", d.contentKey);
    }
    s.endGroup();

    // ©¤©¤ CONTENT ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤
    s.beginGroup("CONTENT");
    for (auto it = contents.cbegin(); it != contents.cend(); ++it)
        s.setValue(it.key(), it.value());
    s.endGroup();

    s.sync();
    return s.status() == QSettings::NoError;
}

bool BoxFileManager::load(const QString& filePath,
    QList<DesktopItemData>& items,
    QMap<QString, QString>& contents)
{
    if (!QFileInfo::exists(filePath))
        return false;

    QSettings s(filePath, QSettings::IniFormat);
    s.setIniCodec("UTF-8");

    // ©¤©¤ LAYOUT ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤
    s.beginGroup("LAYOUT");
    const int count = s.value("count", 0).toInt();
    items.clear();
    items.reserve(count);

    for (int i = 0; i < count; ++i)
    {
        const QString pfx = QString("item%1").arg(i);
        DesktopItemData d;
        d.name = s.value(pfx + ".name").toString();
        d.path = s.value(pfx + ".path").toString();
        d.row = s.value(pfx + ".row", 0).toInt();
        d.col = s.value(pfx + ".col", 0).toInt();
        d.type = s.value(pfx + ".type", "file").toString();
        d.contentKey = s.value(pfx + ".contentKey").toString();
        items.append(d);
    }
    s.endGroup();

    // ©¤©¤ CONTENT ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤
    s.beginGroup("CONTENT");
    contents.clear();
    const QStringList keys = s.childKeys();
    for (const QString& key : keys)
        contents[key] = s.value(key).toString();
    s.endGroup();

    return s.status() == QSettings::NoError;
}