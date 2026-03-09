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

private:
    static inline QSize m_resolution{defaultWidth, defaultHeight};
    static inline qreal m_scaleFactor = 1.0;
    static inline QString m_redisHost = QStringLiteral("192.168.7.1");
    static inline int m_redisPort = 6379;
};
