#pragma once

#include <QObject>
#include <QString>

class SerialNumberService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString serialNumber READ serialNumber CONSTANT)
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit SerialNumberService(QObject *parent = nullptr);

    QString serialNumber() const { return m_serialNumber; }
    bool available() const { return !m_serialNumber.isEmpty(); }

private:
    void readSerialNumber();
    QString m_serialNumber;
};
