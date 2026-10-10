#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QStringList>
namespace RideGroupJson {
bool parse(const QByteArray &bytes, QJsonObject &object);
bool keys(const QJsonObject &object, const QStringList &expected);
bool counter(const QJsonValue &value, bool zero = true);
bool uuid(const QJsonValue &value);
} // namespace RideGroupJson
