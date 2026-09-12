#pragma once

#include <QByteArray>
#include <QString>

// Route line styling, read from a style JSON's metadata section so a style can
// carry its own route colours instead of them being compiled into QML and the
// route-layer injection.
struct MapRouteStyle {
    QString fillColor = QStringLiteral("#42A5F5");
    QString borderColor = QStringLiteral("#1565C0");
    int fillWidth = 7;
    int borderWidth = 11;
};

// Missing, malformed or nonsensical keys keep the defaults above.
MapRouteStyle parseMapRouteStyle(const QByteArray &styleJson);
