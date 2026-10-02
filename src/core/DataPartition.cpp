#include "DataPartition.h"

#include <QDebug>
#include <QFile>

#include <sys/stat.h>

#include <utility>

const QString DataPartition::Root = QStringLiteral("/data");

bool DataPartition::isMountPoint(const QString &path)
{
    struct stat self;
    struct stat parent;
    if (::stat(QFile::encodeName(path).constData(), &self) != 0)
        return false;
    if (::stat(QFile::encodeName(path + QStringLiteral("/..")).constData(), &parent) != 0)
        return false;
    return self.st_dev != parent.st_dev;
}

bool DataPartition::probe()
{
#if defined(Q_OS_LINUX) && !defined(DESKTOP_MODE)
    return isMountPoint(Root);
#else
    return true;
#endif
}

DataPartition::DataPartition(QObject *parent)
    : DataPartition(Probe{}, parent)
{
}

DataPartition::DataPartition(Probe probe, QObject *parent)
    : QObject(parent)
    , m_probe(std::move(probe))
    , m_mounted(m_probe ? m_probe() : DataPartition::probe())
{
    if (!m_mounted) {
        qDebug() << "DataPartition:" << Root << "not mounted yet, deferring writes";
        startProbe();
    }
}

void DataPartition::startProbe()
{
    m_probeTimer = new QTimer(this);
    m_probeTimer->setInterval(250);
    connect(m_probeTimer, &QTimer::timeout, this, &DataPartition::refresh);
    m_probeTimer->start();
}

void DataPartition::refresh()
{
    if (m_mounted)
        return;
    const bool mounted = m_probe ? m_probe() : DataPartition::probe();
    if (!mounted)
        return;
    m_mounted = true;
    if (m_probeTimer)
        m_probeTimer->stop();
    qDebug() << "DataPartition:" << Root << "mounted";
    emit becameMounted();
}
