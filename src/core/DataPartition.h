#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>

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

    using Probe = std::function<bool()>;

    explicit DataPartition(QObject *parent = nullptr);
    explicit DataPartition(Probe probe, QObject *parent = nullptr);

    bool mounted() const { return m_mounted; }

    // Emits becameMounted() on the false->true edge.
    void refresh();

signals:
    void becameMounted();

private:
    void startProbe();

    Probe m_probe;
    QTimer *m_probeTimer = nullptr;
    bool m_mounted = false;
};
