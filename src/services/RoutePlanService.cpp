#include "RoutePlanService.h"

#include "core/AppConfig.h"
#include "repositories/MdbRepository.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include <initializer_list>

namespace {

const QString kSettings = QStringLiteral("settings");

QString prefix()
{
    return QString::fromLatin1(AppConfig::routePlanPrefix);
}

// Accept a coordinate written as a JSON number or as a string. External
// writers differ: uplink serializes numbers, some CLI paths send strings.
bool jsonCoordinate(const QJsonValue &value, double &out)
{
    if (value.isDouble()) {
        out = value.toDouble();
        return std::isfinite(out);
    }
    if (value.isString()) {
        bool ok = false;
        out = value.toString().toDouble(&ok);
        return ok;
    }
    return false;
}

// First present key wins. Channels differ on names: the canonical wire shape
// uses lat/lon, while cloud and app payloads often send latitude/longitude and
// label or name.
QJsonValue firstValue(const QJsonObject &object,
                      std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        const QJsonValue value = object.value(QLatin1String(key));
        if (!value.isUndefined() && !value.isNull())
            return value;
    }
    return {};
}

} // namespace

RoutePlanService::RoutePlanService(MdbRepository *repo)
    : m_repo(repo)
{
}

QString RoutePlanService::fieldKey(int index, const QString &field) const
{
    return QStringLiteral("%1.%2.%3").arg(prefix()).arg(index).arg(field);
}

void RoutePlanService::removeRecord(int index)
{
    for (const auto &field : {QStringLiteral("latitude"), QStringLiteral("longitude"),
                              QStringLiteral("label"), QStringLiteral("reached")}) {
        m_repo->hdel(kSettings, fieldKey(index, field));
    }
    // Publish so settings-service rewrites TOML without this record.
    m_repo->publish(kSettings, QStringLiteral("%1.%2").arg(prefix()).arg(index));
}

RoutePlan RoutePlanService::load()
{
    RoutePlan plan;

    for (int i = 0; i < MaxStops; ++i) {
        const QString latRaw = m_repo->get(kSettings, fieldKey(i, QStringLiteral("latitude")));
        const QString lonRaw = m_repo->get(kSettings, fieldKey(i, QStringLiteral("longitude")));
        if (latRaw.isEmpty() || lonRaw.isEmpty())
            continue;

        bool latOk = false;
        bool lonOk = false;
        const double lat = latRaw.toDouble(&latOk);
        const double lon = lonRaw.toDouble(&lonOk);
        RouteStop stop;
        stop.position = {lat, lon};
        if (!latOk || !lonOk || !stop.position.isValid())
            continue;

        stop.label = m_repo->get(kSettings, fieldKey(i, QStringLiteral("label")));
        stop.reached = m_repo->get(kSettings, fieldKey(i, QStringLiteral("reached")))
                       == QLatin1String("true");
        plan.stops.append(stop);
    }

    m_savedCount = plan.stops.size();

    const QString stepRaw = m_repo->get(
        kSettings, QStringLiteral("%1.current-step").arg(prefix()));
    plan.currentStep = stepRaw.toInt();
    plan.keepCurrentStop = m_repo->get(
        kSettings, QStringLiteral("%1.keep-current-stop").arg(prefix())) == QLatin1String("true");

    m_loadedActive = m_repo->get(
        kSettings, QStringLiteral("%1.active").arg(prefix())) == QLatin1String("true");

    assignIds(plan.stops);
    plan.clampStep();
    if (plan.atLastStop())
        plan.keepCurrentStop = false;
    return plan;
}

bool RoutePlanService::save(const RoutePlan &plan, bool active)
{
    RoutePlan p = plan;
    p.clampStep();
    const int count = p.stops.size();

    for (int i = 0; i < count; ++i) {
        const RouteStop &stop = p.stops.at(i);
        m_repo->set(kSettings, fieldKey(i, QStringLiteral("latitude")),
                    QString::number(stop.position.latitude, 'f', 7), false);
        m_repo->set(kSettings, fieldKey(i, QStringLiteral("longitude")),
                    QString::number(stop.position.longitude, 'f', 7), false);
        m_repo->set(kSettings, fieldKey(i, QStringLiteral("label")), stop.label, false);
        m_repo->set(kSettings, fieldKey(i, QStringLiteral("reached")),
                    stop.reached ? QStringLiteral("true") : QStringLiteral("false"), false);
    }

    // Drop records left behind by a shorter plan. Tracked because Redis is the
    // only copy and scanning MaxStops keys on every step change is wasteful.
    for (int i = count; i < m_savedCount; ++i)
        removeRecord(i);
    m_savedCount = count;

    const QString stepKey = QStringLiteral("%1.current-step").arg(prefix());
    const QString activeKey = QStringLiteral("%1.active").arg(prefix());
    const QString keepKey = QStringLiteral("%1.keep-current-stop").arg(prefix());
    const QString updatedKey = QStringLiteral("%1.updated-at").arg(prefix());
    m_repo->set(kSettings, stepKey, QString::number(p.currentStep), false);
    m_repo->set(kSettings, activeKey,
                active ? QStringLiteral("true") : QStringLiteral("false"), false);
    m_repo->set(kSettings, keepKey,
                p.keepCurrentStop ? QStringLiteral("true") : QStringLiteral("false"), false);
    m_repo->set(kSettings, updatedKey,
                QDateTime::currentDateTimeUtc().toString(Qt::ISODate), false);

    // One publish per record prefix so settings-service persists the whole
    // record (isIndexedRecordNotification), plus the scalar keys.
    for (int i = 0; i < count; ++i)
        m_repo->publish(kSettings, QStringLiteral("%1.%2").arg(prefix()).arg(i));
    m_repo->publish(kSettings, stepKey);
    m_repo->publish(kSettings, activeKey);
    m_repo->publish(kSettings, keepKey);
    m_repo->publish(kSettings, updatedKey);
    return true;
}

bool RoutePlanService::clear()
{
    for (int i = 0; i < m_savedCount; ++i)
        removeRecord(i);
    m_savedCount = 0;

    for (const auto &suffix : {QStringLiteral("current-step"), QStringLiteral("active"),
                               QStringLiteral("keep-current-stop"), QStringLiteral("updated-at")}) {
        const QString key = QStringLiteral("%1.%2").arg(prefix()).arg(suffix);
        m_repo->hdel(kSettings, key);
        m_repo->publish(kSettings, key);
    }
    return true;
}

void RoutePlanService::assignIds(QList<RouteStop> &stops)
{
    for (int i = 0; i < stops.size(); ++i)
        stops[i].id = i + 1;
}

QString RoutePlanService::serializeWaypoints(const QList<RouteStop> &stops)
{
    QJsonArray array;
    for (const RouteStop &stop : stops) {
        if (!stop.position.isValid())
            continue;
        QJsonObject object;
        object[QStringLiteral("lat")] = stop.position.latitude;
        object[QStringLiteral("lon")] = stop.position.longitude;
        if (!stop.label.isEmpty())
            object[QStringLiteral("label")] = stop.label;
        array.append(object);
    }
    if (array.isEmpty())
        return {};
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QList<RouteStop> RoutePlanService::parseWaypoints(const QString &json, bool *ok)
{
    if (ok)
        *ok = false;

    QList<RouteStop> stops;
    const QString trimmed = json.trimmed();
    if (trimmed.isEmpty())
        return stops;

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray())
        return stops;

    for (const QJsonValue &value : doc.array()) {
        if (!value.isObject())
            continue;
        const QJsonObject object = value.toObject();

        double lat = 0;
        double lon = 0;
        if (!jsonCoordinate(firstValue(object, {"lat", "latitude"}), lat)
            || !jsonCoordinate(firstValue(object, {"lon", "longitude"}), lon)) {
            continue;
        }

        RouteStop stop;
        stop.position = {lat, lon};
        if (!stop.position.isValid())
            continue;
        stop.label = firstValue(object, {"label", "name"}).toString();
        stops.append(stop);
    }

    if (stops.isEmpty())
        return stops;

    assignIds(stops);
    if (ok)
        *ok = true;
    return stops;
}
