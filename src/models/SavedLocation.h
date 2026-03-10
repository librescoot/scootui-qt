#pragma once

#include <QString>
#include <QDateTime>

struct SavedLocation {
    int id = -1;
    double latitude = 0;
    double longitude = 0;
    QString label;
    QDateTime createdAt;
    QDateTime lastUsedAt;

    bool isValid() const { return id >= 0 && latitude != 0 && longitude != 0; }
};
