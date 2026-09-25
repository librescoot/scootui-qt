#include "core/ShortcutMenuItems.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <algorithm>

namespace {

const QStringList &fixedActionIds()
{
    static const QStringList ids = {
        QStringLiteral("view"),
        QStringLiteral("theme"),
        QStringLiteral("debug-overlay"),
        QStringLiteral("motion-debug"),
        QStringLiteral("route-overview"),
        QStringLiteral("road-blocked"),
        QStringLiteral("skip-stop"),
        QStringLiteral("stop-navigation"),
    };
    return ids;
}

const QStringList &allowedIcons()
{
    static const QStringList icons = {
        QStringLiteral("place"),
        QStringLiteral("home"),
        QStringLiteral("work"),
        QStringLiteral("favorite"),
    };
    return icons;
}

QList<int> destinationIndices(const QStringList &items)
{
    QList<int> indices;
    for (qsizetype i = 0; i < items.size(); ++i) {
        if (ShortcutMenuItems::isDestination(items.at(i)))
            indices.append(int(i));
    }
    return indices;
}

int destinationPositionOf(const QStringList &items, const QString &uuid)
{
    const QList<int> destinations = destinationIndices(items);
    for (int position = 0; position < destinations.size(); ++position) {
        if (ShortcutMenuItems::uuidEquals(items.at(destinations.at(position))
                                               .section(QLatin1Char(':'), 1, 1),
                                           uuid))
            return position;
    }
    return -1;
}

} // namespace

namespace ShortcutMenuItems {

QStringList defaultItems()
{
    return fixedActionIds();
}

QStringList parse(const QString &json, bool *ok)
{
    if (ok)
        *ok = false;
    if (json.trimmed().isEmpty())
        return {};

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray())
        return {};

    QStringList items;
    const QJsonArray array = doc.array();
    for (const QJsonValue &value : array) {
        if (!value.isString())
            return {};
        items.append(value.toString());
    }
    if (ok)
        *ok = true;
    return items;
}

QString serialize(const QStringList &items)
{
    const QJsonDocument doc(QJsonArray::fromStringList(items));
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

bool isDestination(const QString &token, QString *uuid, QString *icon)
{
    if (!token.startsWith(QLatin1String("destination:")))
        return false;
    const QStringList parts = token.split(QLatin1Char(':'));
    if (parts.size() != 3 || parts.at(1).isEmpty() || parts.at(2).isEmpty())
        return false;
    if (QUuid(parts.at(1)).isNull())
        return false;
    if (uuid)
        *uuid = parts.at(1);
    if (icon)
        *icon = parts.at(2);
    return true;
}

bool uuidEquals(const QString &left, const QString &right)
{
    const QUuid a(left);
    const QUuid b(right);
    return !a.isNull() && a == b;
}

QString destinationToken(const QString &uuid, const QString &icon)
{
    const QString safeIcon = allowedIcons().contains(icon) ? icon : allowedIcons().front();
    return QStringLiteral("destination:%1:%2").arg(uuid, safeIcon);
}

QStringList destinationUuids(const QStringList &items)
{
    QStringList uuids;
    for (const QString &token : items) {
        QString uuid;
        if (isDestination(token, &uuid))
            uuids.append(uuid);
    }
    return uuids;
}

QString destinationTokenAt(const QStringList &items, int slot)
{
    const QList<int> destinations = destinationIndices(items);
    if (slot < 1 || slot > destinations.size())
        return {};
    return items.at(destinations.at(slot - 1));
}

QStringList setDestinationSlot(const QStringList &items, const QString &uuid, int slot)
{
    if (slot < 1 || uuid.isEmpty())
        return items;

    QStringList result = items;
    const QList<int> destinations = destinationIndices(result);
    const int position = destinationPositionOf(result, uuid);

    if (position >= 0) {
        const int target = std::min(slot - 1, int(destinations.size()) - 1);
        if (target == position)
            return result;
        const int from = destinations.at(position);
        const int to = destinations.at(target);
        std::swap(result[from], result[to]);
        return result;
    }

    const int insertAt = std::min(slot - 1, int(destinations.size()));
    if (insertAt < destinations.size()) {
        // The holder of the requested slot is replaced; the displaced token
        // has no slot to fall back to and leaves the list.
        result.removeAt(destinations.at(insertAt));
        result.insert(destinations.at(insertAt), destinationToken(uuid, QStringLiteral("place")));
    } else {
        const int index = destinations.isEmpty()
            ? result.size()
            : destinations.last() + (insertAt == destinations.size() ? 1 : 0);
        result.insert(std::min(index, int(result.size())),
                      destinationToken(uuid, QStringLiteral("place")));
    }
    return result;
}

QStringList clearDestinationSlot(const QStringList &items, int slot)
{
    const QList<int> destinations = destinationIndices(items);
    if (slot < 1 || slot > destinations.size())
        return items;
    QStringList result = items;
    result.removeAt(destinations.at(slot - 1));
    return result;
}

QStringList setDestinationIcon(const QStringList &items, const QString &uuid,
                               const QString &icon)
{
    for (const QString &token : items) {
        QString tokenUuid;
        if (!isDestination(token, &tokenUuid) || !uuidEquals(tokenUuid, uuid))
            continue;
        QStringList result = items;
        result[result.indexOf(token)] = destinationToken(uuid, icon);
        return result;
    }
    return items;
}

QStringList withoutDestination(const QStringList &items, const QString &uuid)
{
    QStringList result = items;
    for (qsizetype i = result.size() - 1; i >= 0; --i) {
        QString tokenUuid;
        if (isDestination(result.at(i), &tokenUuid) && uuidEquals(tokenUuid, uuid))
            result.removeAt(i);
    }
    return result;
}

} // namespace ShortcutMenuItems
