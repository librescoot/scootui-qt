#include "EngineStore.h"

EngineStore::EngineStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
}

SyncSettings EngineStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("engine-ecu"),
        200,
        {
            {QStringLiteral("state"), QStringLiteral("state")},
            {QStringLiteral("kers"), QStringLiteral("kers")},
            {QStringLiteral("kersReasonOff"), QStringLiteral("kers-reason-off")},
            {QStringLiteral("motorVoltage"), QStringLiteral("motor:voltage")},
            {QStringLiteral("motorCurrent"), QStringLiteral("motor:current")},
            {QStringLiteral("rpm"), QStringLiteral("rpm")},
            {QStringLiteral("speed"), QStringLiteral("speed")},
            {QStringLiteral("rawSpeed"), QStringLiteral("raw-speed")},
            {QStringLiteral("throttle"), QStringLiteral("throttle")},
            {QStringLiteral("firmwareVersion"), QStringLiteral("fw-version")},
            {QStringLiteral("odometer"), QStringLiteral("odometer")},
            {QStringLiteral("temperature"), QStringLiteral("temperature")},
        },
        {}, // no set fields
        {}  // no discriminator
    };
}

void EngineStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("state")) {
        auto v = ScootEnums::parseToggle(value);
        if (v != m_powerState) { m_powerState = v; emit powerStateChanged(); }
    } else if (variable == QLatin1String("kers")) {
        auto v = ScootEnums::parseToggle(value);
        if (v != m_kers) { m_kers = v; emit kersChanged(); }
    } else if (variable == QLatin1String("kers-reason-off")) {
        if (value != m_kersReasonOff) { m_kersReasonOff = value; emit kersReasonOffChanged(); }
    } else if (variable == QLatin1String("motor:voltage")) {
        auto v = value.toDouble();
        if (v != m_motorVoltage) { m_motorVoltage = v; emit motorVoltageChanged(); }
    } else if (variable == QLatin1String("motor:current")) {
        auto v = value.toDouble();
        if (v != m_motorCurrent) { m_motorCurrent = v; emit motorCurrentChanged(); }
    } else if (variable == QLatin1String("rpm")) {
        auto v = value.toDouble();
        if (v != m_rpm) { m_rpm = v; emit rpmChanged(); }
    } else if (variable == QLatin1String("speed")) {
        auto v = value.toDouble();
        if (v != m_speed) { m_speed = v; emit speedChanged(); }
    } else if (variable == QLatin1String("raw-speed")) {
        auto v = value.toDouble();
        if (v != m_rawSpeed || !m_hasRawSpeed) {
            m_rawSpeed = v;
            m_hasRawSpeed = true;
            emit rawSpeedChanged();
        }
    } else if (variable == QLatin1String("throttle")) {
        auto v = ScootEnums::parseToggle(value);
        if (v != m_throttle) { m_throttle = v; emit throttleChanged(); }
    } else if (variable == QLatin1String("fw-version")) {
        if (value != m_firmwareVersion) { m_firmwareVersion = value; emit firmwareVersionChanged(); }
    } else if (variable == QLatin1String("odometer")) {
        auto v = value.toDouble();
        if (v != m_odometer) { m_odometer = v; emit odometerChanged(); }
    } else if (variable == QLatin1String("temperature")) {
        auto v = value.toDouble();
        if (v != m_temperature) { m_temperature = v; emit temperatureChanged(); }
    }
}
