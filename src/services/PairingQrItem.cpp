#include "PairingQrItem.h"
#include "qrcodegen.hpp"
#include <QPainter>
#include <QRegularExpression>

PairingQrItem::PairingQrItem(QQuickItem *parent) : QQuickPaintedItem(parent) {}

void PairingQrItem::setCode(const QString &code)
{
    if (m_code == code) return;
    m_code = code;
    update();
    emit codeChanged();
}

void PairingQrItem::paint(QPainter *painter)
{
    static const QRegularExpression format(QStringLiteral("^[0-9ABCDEFGHJKMNPQRSTVWXYZ]{5}-[0-9ABCDEFGHJKMNPQRSTVWXYZ]{5}$"));
    if (m_code.size() != 11 || !format.match(m_code).hasMatch()) return;
    const auto text = (QStringLiteral("https://connect.librescoot.org/pair#code=") + m_code).toStdString();
    const auto qr = qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
    const int module = int(qMin(width(), height())) / (qr.getSize() + 8);
    if (module < 1) return;
    const int side = (qr.getSize() + 8) * module;
    const int left = int(width() - side) / 2, top = int(height() - side) / 2;
    painter->fillRect(left, top, side, side, Qt::white);
    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::black);
    for (int y = 0; y < qr.getSize(); ++y)
        for (int x = 0; x < qr.getSize(); ++x)
            if (qr.getModule(x, y)) painter->drawRect(left + (x + 4) * module, top + (y + 4) * module, module, module);
}
