#pragma once

#include <QSize>
#include <QString>

class EnvConfig {
public:
    static constexpr int defaultWidth = 480;
    static constexpr int defaultHeight = 480;

    static void initialize();

    static QSize resolution() { return m_resolution; }
    static qreal scaleFactor() { return m_scaleFactor; }
    static QString redisHost() { return m_redisHost; }
    static int redisPort() { return m_redisPort; }

    // SCOOTUI_SIMULATOR decides whether the simulator panel runs, independently
    // of which repository backs it. Unset it follows the backend, which is what
    // the Redis host used to decide on its own: on for the in-memory
    // repository, off against a real Redis. Set it to run the panel against a
    // Redis, whether that is a dedicated one with services alongside or a
    // vehicle being mirrored.
    static bool simulatorEnabled(bool inMemoryBackend)
    {
        return m_simulatorOverride < 0 ? inMemoryBackend : m_simulatorOverride > 0;
    }

    // SCOOTUI_HIDE_USB_WARNING=1 suppresses the "USB connection interrupted"
    // toast. Dev-only escape hatch: a dev box plugged into the MDB's single
    // USB port displaces the DBC onto the PPP backup link by design (see
    // CLAUDE.md), which is expected there but a real signal on a real
    // vehicle, so this is never set in production.
    static bool hideUsbWarning() { return m_hideUsbWarning; }

private:
    static inline QSize m_resolution{defaultWidth, defaultHeight};
    static inline qreal m_scaleFactor = 1.0;
    static inline QString m_redisHost = QStringLiteral("192.168.7.1");
    static inline int m_redisPort = 6379;
    // -1 unset, 0 forced off, 1 forced on
    static inline int m_simulatorOverride = -1;
    static inline bool m_hideUsbWarning = false;
};
