#pragma once

#include <QObject>
#include <QString>

// The DBC starts the dashboard before /data is mounted; anything written under
// it until then lands on the rootfs and is shadowed once the mount happens.
class DataPartition : public QObject
{
    Q_OBJECT

public:
    static const QString Root;

    // st_dev differs from the parent's once a filesystem is mounted at path.
    static bool isMountPoint(const QString &path);

    // Always true on builds that keep their state outside Root.
    static bool probe();

    explicit DataPartition(QObject *parent = nullptr);

    bool mounted() const { return m_mounted; }

    // Emits becameMounted() on the false->true edge.
    void refresh();

signals:
    void becameMounted();

private:
    bool m_mounted = false;
};
