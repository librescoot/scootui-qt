#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlengine.h>

class SerialNumberService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString serialNumber READ serialNumber CONSTANT)
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit SerialNumberService(QObject *parent = nullptr);

    QString serialNumber() const { return m_serialNumber; }
    bool available() const { return !m_serialNumber.isEmpty(); }

private:
    void readSerialNumber();
    QString m_serialNumber;

public:
    // Application owns the instance and wires its dependencies before the engine
    // loads. create() hands QML that object instead of a default-constructed one.
    static SerialNumberService *create(QQmlEngine *, QJSEngine *)
    {
        Q_ASSERT(s_qmlInstance);
        QJSEngine::setObjectOwnership(s_qmlInstance, QJSEngine::CppOwnership);
        return s_qmlInstance;
    }
    static void setQmlInstance(SerialNumberService *instance) { s_qmlInstance = instance; }

private:
    static inline SerialNumberService *s_qmlInstance = nullptr;
};
