#include "SerialNumberService.h"

#include <QFile>
#include <QDebug>

SerialNumberService::SerialNumberService(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    readSerialNumber();
}

void SerialNumberService::readSerialNumber()
{
    // Primary path: i.MX6 soc serial number (DBC)
    QFile socFile(QStringLiteral("/sys/devices/soc0/serial_number"));
    if (socFile.open(QIODevice::ReadOnly)) {
        m_serialNumber = QString::fromUtf8(socFile.readAll()).trimmed();
        if (!m_serialNumber.isEmpty()) {
            qDebug() << "Serial number (soc0):" << m_serialNumber;
            return;
        }
    }

    // Fallback: OTP fuse registers (CFG0 + CFG1)
    QFile cfg0File(QStringLiteral("/sys/fsl_otp/HW_OCOTP_CFG0"));
    QFile cfg1File(QStringLiteral("/sys/fsl_otp/HW_OCOTP_CFG1"));

    if (cfg0File.open(QIODevice::ReadOnly) && cfg1File.open(QIODevice::ReadOnly)) {
        bool ok0 = false, ok1 = false;
        QString cfg0Str = QString::fromUtf8(cfg0File.readAll()).trimmed();
        QString cfg1Str = QString::fromUtf8(cfg1File.readAll()).trimmed();

        quint64 cfg0 = cfg0Str.toULongLong(&ok0, 16);
        quint64 cfg1 = cfg1Str.toULongLong(&ok1, 16);

        if (ok0 && ok1) {
            m_serialNumber = QStringLiteral("%1").arg(cfg0 + cfg1, 16, 16, QLatin1Char('0')).toUpper();
            qDebug() << "Serial number (OTP fuses):" << m_serialNumber;
            return;
        }
    }

    qDebug() << "Serial number: not available";
}
