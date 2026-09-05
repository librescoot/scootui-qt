#include "EnvConfig.h"

#include <QProcessEnvironment>
#include <QDebug>
#include <algorithm>

void EnvConfig::initialize()
{
    const auto env = QProcessEnvironment::systemEnvironment();

    // Simulator panel: SCOOTUI_SIMULATOR=1/true/on, or 0/false/off
    const QString simStr = env.value(QStringLiteral("SCOOTUI_SIMULATOR")).trimmed().toLower();
    if (!simStr.isEmpty()) {
        const bool on = (simStr == QLatin1String("1") || simStr == QLatin1String("true")
                         || simStr == QLatin1String("on") || simStr == QLatin1String("yes"));
        const bool off = (simStr == QLatin1String("0") || simStr == QLatin1String("false")
                          || simStr == QLatin1String("off") || simStr == QLatin1String("no"));
        if (on || off) {
            m_simulatorOverride = on ? 1 : 0;
            qDebug() << "Simulator panel forced" << (on ? "on" : "off") << "by environment";
        } else {
            qWarning() << "Ignoring unrecognised SCOOTUI_SIMULATOR value:" << simStr;
        }
    }

    // Resolution: SCOOTUI_RESOLUTION=widthxheight
    const QString resStr = env.value(QStringLiteral("SCOOTUI_RESOLUTION"));
    if (!resStr.isEmpty()) {
        const auto parts = resStr.toLower().split(QLatin1Char('x'));
        if (parts.size() == 2) {
            bool okW = false, okH = false;
            const int w = parts[0].toInt(&okW);
            const int h = parts[1].toInt(&okH);
            if (okW && okH && w > 0 && h > 0) {
                m_resolution = QSize(w, h);
                const qreal defaultMin = std::min(defaultWidth, defaultHeight);
                const qreal newMin = std::min(w, h);
                m_scaleFactor = newMin / defaultMin;
                qDebug() << "Resolution:" << m_resolution << "Scale:" << m_scaleFactor;
            } else {
                qWarning() << "Invalid SCOOTUI_RESOLUTION:" << resStr;
            }
        }
    }

    // Hide the USB-backup-connection toast: SCOOTUI_HIDE_USB_WARNING=1/true/on
    const QString hideUsbStr = env.value(QStringLiteral("SCOOTUI_HIDE_USB_WARNING")).trimmed().toLower();
    if (hideUsbStr == QLatin1String("1") || hideUsbStr == QLatin1String("true")
        || hideUsbStr == QLatin1String("on") || hideUsbStr == QLatin1String("yes")) {
        m_hideUsbWarning = true;
        qDebug() << "USB connection warning suppressed by environment";
    }

    // Redis host: SCOOTUI_REDIS_HOST=host:port or just host
    const QString redisStr = env.value(QStringLiteral("SCOOTUI_REDIS_HOST"));
    if (!redisStr.isEmpty()) {
        if (redisStr.contains(QLatin1Char(':'))) {
            const auto parts = redisStr.split(QLatin1Char(':'));
            m_redisHost = parts[0];
            bool ok = false;
            const int port = parts[1].toInt(&ok);
            if (ok) m_redisPort = port;
        } else {
            m_redisHost = redisStr;
        }
        qDebug() << "Redis:" << m_redisHost << ":" << m_redisPort;
    }
}
