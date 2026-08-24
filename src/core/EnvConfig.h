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

private:
    static inline QSize m_resolution{defaultWidth, defaultHeight};
    static inline qreal m_scaleFactor = 1.0;
    static inline QString m_redisHost = QStringLiteral("192.168.7.1");
    static inline int m_redisPort = 6379;
    // -1 unset, 0 forced off, 1 forced on
    static inline int m_simulatorOverride = -1;
};
