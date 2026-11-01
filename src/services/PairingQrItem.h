#pragma once

#include <QQuickPaintedItem>

class PairingQrItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QString code READ code WRITE setCode NOTIFY codeChanged)
public:
    explicit PairingQrItem(QQuickItem *parent = nullptr);
    QString code() const { return m_code; }
    void setCode(const QString &code);
    void paint(QPainter *painter) override;
signals:
    void codeChanged();
private:
    QString m_code;
};
