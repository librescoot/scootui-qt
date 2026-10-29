#include "BootTransition.h"

#include <QFile>
#include <QDebug>
#include <utility>

BootTransition::BootTransition(QObject *parent, QString alphaPath, QString helper, int durationMs)
    : QObject(parent), m_alphaPath(std::move(alphaPath)), m_helper(std::move(helper)),
      m_durationMs(durationMs)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        qWarning() << "Boot crossfade timed out";
        m_process.kill();
        finish(false);
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        qWarning() << "Boot crossfade helper failed:" << m_process.errorString();
        finish(false);
    });
    connect(&m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus status) {
        finish(status == QProcess::NormalExit && code == 0);
    });
}

void BootTransition::start()
{
    if (m_started)
        return;
    m_started = true;
    m_process.setProcessChannelMode(QProcess::ForwardedChannels);
    m_process.setProgram(m_helper);
    m_process.setArguments({QStringLiteral("fade"), QStringLiteral("0"),
                            QStringLiteral("255"), QString::number(m_durationMs)});
    m_timeout.start(m_durationMs + 1000);
    m_process.start();
}

void BootTransition::finish(bool faded)
{
    if (m_finished)
        return;
    m_finished = true;
    m_timeout.stop();
    if (!faded && m_process.state() != QProcess::NotRunning)
        m_process.kill();

    bool visible = faded;
    if (!visible) {
        // Never stop the splash while a failed helper leaves the UI transparent.
        QFile alpha(m_alphaPath);
        visible = alpha.open(QIODevice::WriteOnly)
            && alpha.write("255\n") == 4 && alpha.flush();
        qWarning() << "Boot crossfade direct reveal:" << visible;
    }
    emit finished(visible);
}
