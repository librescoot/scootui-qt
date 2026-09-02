#include "DataPartition.h"

#include <QDebug>
#include <QFile>

#include <sys/stat.h>

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
    : QObject(parent)
    , m_mounted(probe())
{
    if (!m_mounted)
        qDebug() << "DataPartition:" << Root << "not mounted yet, deferring writes";
}

void DataPartition::refresh()
{
    if (m_mounted || !probe())
        return;
    m_mounted = true;
    qDebug() << "DataPartition:" << Root << "mounted";
    emit becameMounted();
}
