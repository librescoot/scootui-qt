#pragma once

#include <QObject>
#include <QtQml/qqmlengine.h>

#include "BatteryStore.h"

// The seatbox has one BatteryStore per slot, so BatteryStore cannot be a
// singleton itself. This exposes both instances under a single type QML can
// resolve, which is what qmltc needs to compile the bindings that read them.
class Batteries : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(BatteryStore *slot0 READ slot0 CONSTANT)
    Q_PROPERTY(BatteryStore *slot1 READ slot1 CONSTANT)

public:
    // parent is deliberately not defaulted: a default-constructible type makes Qt
    // pick SingletonConstructionMode::Constructor and build its own instance
    // instead of calling create(), which would hand QML an unwired object.
    explicit Batteries(BatteryStore *slot0, BatteryStore *slot1, QObject *parent)
        : QObject(parent)
        , m_slot0(slot0)
        , m_slot1(slot1)
    {
    }

    BatteryStore *slot0() const { return m_slot0; }
    BatteryStore *slot1() const { return m_slot1; }

    static Batteries *create(QQmlEngine *, QJSEngine *)
    {
        Q_ASSERT(s_qmlInstance);
        QJSEngine::setObjectOwnership(s_qmlInstance, QJSEngine::CppOwnership);
        return s_qmlInstance;
    }
    static void setQmlInstance(Batteries *instance) { s_qmlInstance = instance; }

private:
    static inline Batteries *s_qmlInstance = nullptr;

    BatteryStore *m_slot0 = nullptr;
    BatteryStore *m_slot1 = nullptr;
};
