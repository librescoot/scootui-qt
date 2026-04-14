#include "GestureSynth.h"

GestureSynth::GestureSynth(QObject *parent)
    : QObject(parent)
{
}

void GestureSynth::onChange(const QString &name, bool pressed)
{
    State &s = m_state[name];

    if (!s.longTapTimer) {
        s.longTapTimer = new QTimer(this);
        s.longTapTimer->setSingleShot(true);
        s.longTapTimer->setInterval(LONG_TAP_MS);
        connect(s.longTapTimer, &QTimer::timeout, this, [this, name]() {
            State &st = m_state[name];
            if (st.pressed) {
                st.longTapFired = true;
                st.lastTapAt = QDateTime();
                emit event(name + QStringLiteral(":long-tap"));
            }
        });

        s.holdTimer = new QTimer(this);
        s.holdTimer->setSingleShot(true);
        s.holdTimer->setInterval(HOLD_MS);
        connect(s.holdTimer, &QTimer::timeout, this, [this, name]() {
            State &st = m_state[name];
            if (st.pressed) {
                st.holdFired = true;
                st.lastTapAt = QDateTime();
                emit event(name + QStringLiteral(":hold"));
            }
        });
    }

    if (pressed && !s.pressed) {
        s.pressed = true;
        s.longTapFired = false;
        s.holdFired = false;
        emit event(name + QStringLiteral(":press"));
        s.longTapTimer->start();
        s.holdTimer->start();
    } else if (!pressed && s.pressed) {
        s.pressed = false;
        s.longTapTimer->stop();
        s.holdTimer->stop();
        emit event(name + QStringLiteral(":release"));
        if (!s.longTapFired && !s.holdFired) {
            const QDateTime now = QDateTime::currentDateTime();
            emit event(name + QStringLiteral(":tap"));
            if (s.lastTapAt.isValid() && s.lastTapAt.msecsTo(now) <= DOUBLE_TAP_MS) {
                s.lastTapAt = QDateTime();
                emit event(name + QStringLiteral(":double-tap"));
            } else {
                s.lastTapAt = now;
            }
        }
    }
}
