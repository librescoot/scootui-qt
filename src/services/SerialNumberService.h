#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class SerialNumberService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString serialNumber READ serialNumber CONSTANT)
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit SerialNumberService(QObject *parent = nullptr);
    static SerialNumberService *create(QQmlEngine *, QJSEngine *) { return s_instance; }

    QString serialNumber() const { return m_serialNumber; }
    bool available() const { return !m_serialNumber.isEmpty(); }

private:
    void readSerialNumber();
    QString m_serialNumber;

    static inline SerialNumberService *s_instance = nullptr;
};
