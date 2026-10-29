#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>

class BootTransition : public QObject
{
    Q_OBJECT
public:
    explicit BootTransition(QObject *parent = nullptr,
                            QString alphaPath = QStringLiteral("/sys/class/graphics/fb1/overlay_alpha"),
                            QString helper = QStringLiteral("/usr/bin/imx-overlay-alpha"),
                            int durationMs = 1000);
    void start();

signals:
    void finished(bool visible);

private:
    void finish(bool faded);

    QString m_alphaPath;
    QString m_helper;
    int m_durationMs;
    QProcess m_process;
    QTimer m_timeout;
    bool m_started = false;
    bool m_finished = false;
};
