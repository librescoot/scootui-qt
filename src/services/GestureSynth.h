#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>

// Mirrors vehicle-service's input gesture detector
// (vehicle-service/internal/core/input_gestures.go). Used by the simulator
// so it can publish to the "input-events" pub/sub channel in the same
// shape real vehicle-service does.
class GestureSynth : public QObject
{
    Q_OBJECT

public:
    explicit GestureSynth(QObject *parent = nullptr);

    void onChange(const QString &name, bool pressed);

signals:
    // Emitted as "<name>:press|tap|long-tap|hold|release|double-tap".
    void event(const QString &event);

private:
    static constexpr int LONG_TAP_MS = 800;
    static constexpr int HOLD_MS = 3000;
    static constexpr int DOUBLE_TAP_MS = 800;

    struct State {
        bool pressed = false;
        bool longTapFired = false;
        bool holdFired = false;
        QTimer *longTapTimer = nullptr;
        QTimer *holdTimer = nullptr;
        QDateTime lastTapAt;
    };

    QHash<QString, State> m_state;
};
