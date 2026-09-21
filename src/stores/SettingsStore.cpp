#include "SettingsStore.h"

#include <limits>

namespace {

constexpr qint64 kMaxDurationNanoseconds = std::numeric_limits<qint64>::max();

bool isAsciiDigit(const QChar character)
{
    return character >= QLatin1Char('0') && character <= QLatin1Char('9');
}

bool parseNonnegativeInt64(const QString &value, qint64 &result)
{
    if (value.isEmpty())
        return false;

    qint64 parsed = 0;
    for (const QChar character : value) {
        if (!isAsciiDigit(character))
            return false;
        const qint64 digit = character.unicode() - QLatin1Char('0').unicode();
        if (parsed > (kMaxDurationNanoseconds - digit) / 10)
            return false;
        parsed = parsed * 10 + digit;
    }
    result = parsed;
    return true;
}

bool isCanonicalNonnegativeDecimal(const QString &value)
{
    if (value.isEmpty() || (value.size() > 1 && value.startsWith(QLatin1Char('0'))))
        return false;

    qint64 unused = 0;
    return parseNonnegativeInt64(value, unused);
}

qint64 fractionalNanoseconds(const QString &digits, qint64 unitNanoseconds)
{
    const QByteArray unit = QByteArray::number(unitNanoseconds);
    QByteArray product(digits.size() + unit.size(), 0);
    for (qsizetype i = digits.size() - 1; i >= 0; --i) {
        const int digit = digits.at(i).unicode() - QLatin1Char('0').unicode();
        int carry = 0;
        for (qsizetype j = unit.size() - 1; j >= 0; --j) {
            const int index = i + j + 1;
            const int sum = product.at(index) + digit * (unit.at(j) - '0') + carry;
            product[index] = static_cast<char>(sum % 10);
            carry = sum / 10;
        }
        product[i] = static_cast<char>(product.at(i) + carry);
    }

    qint64 result = 0;
    for (qsizetype i = 0; i < unit.size(); ++i)
        result = result * 10 + product.at(i);
    return result;
}

bool isValidPositiveDuration(const QString &value)
{
    if (value.isEmpty())
        return false;

    qint64 totalNanoseconds = 0;
    qsizetype offset = 0;
    while (offset < value.size()) {
        const qsizetype integerStart = offset;
        while (offset < value.size() && isAsciiDigit(value.at(offset)))
            ++offset;
        const QString integer = value.mid(integerStart, offset - integerStart);
        if (integer.isEmpty()
            || (integer.size() > 1 && integer.startsWith(QLatin1Char('0'))))
            return false;

        QString fraction;
        if (offset < value.size() && value.at(offset) == QLatin1Char('.')) {
            ++offset;
            const qsizetype fractionStart = offset;
            while (offset < value.size() && isAsciiDigit(value.at(offset)))
                ++offset;
            fraction = value.mid(fractionStart, offset - fractionStart);
            if (fraction.isEmpty())
                return false;
        }

        qint64 unitNanoseconds = 0;
        if (value.mid(offset, 2) == QLatin1String("ns")) {
            unitNanoseconds = 1;
            offset += 2;
        } else if (value.mid(offset, 2) == QLatin1String("us")) {
            unitNanoseconds = 1000;
            offset += 2;
        } else if (value.mid(offset, 2) == QLatin1String("ms")) {
            unitNanoseconds = 1000000;
            offset += 2;
        } else if (offset < value.size() && value.at(offset) == QLatin1Char('s')) {
            unitNanoseconds = 1000000000;
            ++offset;
        } else if (offset < value.size() && value.at(offset) == QLatin1Char('m')) {
            unitNanoseconds = 60000000000LL;
            ++offset;
        } else if (offset < value.size() && value.at(offset) == QLatin1Char('h')) {
            unitNanoseconds = 3600000000000LL;
            ++offset;
        } else {
            return false;
        }

        qint64 whole = 0;
        if (!integer.isEmpty() && !parseNonnegativeInt64(integer, whole))
            return false;
        if (whole > kMaxDurationNanoseconds / unitNanoseconds)
            return false;
        qint64 componentNanoseconds = whole * unitNanoseconds;
        if (!fraction.isEmpty()) {
            const qint64 fractional = fractionalNanoseconds(fraction, unitNanoseconds);
            if (componentNanoseconds > kMaxDurationNanoseconds - fractional)
                return false;
            componentNanoseconds += fractional;
        }
        if (totalNanoseconds > kMaxDurationNanoseconds - componentNanoseconds)
            return false;
        totalNanoseconds += componentNanoseconds;
    }
    return totalNanoseconds > 0;
}

} // namespace

SettingsStore::SettingsStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
}

SyncSettings SettingsStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("settings"), 5000,
        {
            {QStringLiteral("theme"), QStringLiteral("dashboard.theme")},
            {QStringLiteral("mode"), QStringLiteral("dashboard.mode")},
            {QStringLiteral("developerMode"), QStringLiteral("scooter.developer-mode")},
            {QStringLiteral("backlightMode"), QStringLiteral("dashboard.backlight-mode")},
            {QStringLiteral("showRawSpeed"), QStringLiteral("dashboard.show-raw-speed")},
            {QStringLiteral("speedometerMaxSpeed"), QStringLiteral("dashboard.speedometer.max-speed")},
            {QStringLiteral("speedometerWarnSpeed"), QStringLiteral("dashboard.speedometer.warn-speed")},
            {QStringLiteral("speedometerOverspeed"), QStringLiteral("dashboard.speedometer.overspeed")},
            {QStringLiteral("speedometerBaseColor"), QStringLiteral("dashboard.speedometer.base-color")},
            {QStringLiteral("speedometerWarnColor"), QStringLiteral("dashboard.speedometer.warn-color")},
            {QStringLiteral("speedometerOverspeedColor"), QStringLiteral("dashboard.speedometer.overspeed-color")},
            {QStringLiteral("batteryDisplayMode"), QStringLiteral("dashboard.battery-display-mode")},
            {QStringLiteral("mapType"), QStringLiteral("dashboard.map.type")},
            {QStringLiteral("mapViewMode"), QStringLiteral("dashboard.map.view-mode")},
            {QStringLiteral("mapNorthOriented"), QStringLiteral("dashboard.map.north-oriented")},
            {QStringLiteral("mapRenderMode"), QStringLiteral("dashboard.map.render-mode")},
            {QStringLiteral("valhallaUrl"), QStringLiteral("dashboard.valhalla-url")},
            {QStringLiteral("routePreference"), QStringLiteral("dashboard.route-preference")},
            {QStringLiteral("avoidCobblestone"), QStringLiteral("dashboard.avoid-cobblestone")},
            {QStringLiteral("language"), QStringLiteral("dashboard.language")},
            {QStringLiteral("powerDisplayMode"), QStringLiteral("dashboard.power-display-mode")},
            {QStringLiteral("blinkerStyle"), QStringLiteral("dashboard.blinker-style")},
            {QStringLiteral("dbcBlinkerLed"), QStringLiteral("scooter.dbc-blinker-led")},
            {QStringLiteral("dualBattery"), QStringLiteral("scooter.dual-battery")},
            {QStringLiteral("hornWhenSeatboxOpen"), QStringLiteral("scooter.horn-when-seatbox-open")},
            {QStringLiteral("showGps"), QStringLiteral("dashboard.show-gps")},
            {QStringLiteral("showBluetooth"), QStringLiteral("dashboard.show-bluetooth")},
            {QStringLiteral("showCloud"), QStringLiteral("dashboard.show-cloud")},
            {QStringLiteral("showInternet"), QStringLiteral("dashboard.show-internet")},
            {QStringLiteral("showClock"), QStringLiteral("dashboard.show-clock")},
            {QStringLiteral("showTemperature"), QStringLiteral("dashboard.show-temperature")},
            {QStringLiteral("showCbBattery"), QStringLiteral("dashboard.show-cb-battery")},
            {QStringLiteral("showAuxBattery"), QStringLiteral("dashboard.show-aux-battery")},
            {QStringLiteral("showRoadName"), QStringLiteral("dashboard.show-road-name")},
            {QStringLiteral("showSpeedLimit"), QStringLiteral("dashboard.show-speed-limit")},
            {QStringLiteral("alarmEnabled"), QStringLiteral("alarm.enabled")},
            {QStringLiteral("alarmHonk"), QStringLiteral("alarm.honk")},
            {QStringLiteral("alarmDuration"), QStringLiteral("alarm.duration")},
            {QStringLiteral("hopOnCombo"), QStringLiteral("dashboard.hop-on-combo")},
            {QStringLiteral("mapCheckForUpdates"), QStringLiteral("dashboard.maps.check-for-updates")},
            {QStringLiteral("mapAutoDownload"), QStringLiteral("dashboard.maps.auto-download")},
            {QStringLiteral("mapTrafficOverlay"), QStringLiteral("dashboard.map.traffic-overlay")},
            {QStringLiteral("milestoneCelebrations"), QStringLiteral("dashboard.milestone-celebrations")},
            {QStringLiteral("serviceActive"), QStringLiteral("dashboard.service-mode-active")},
            {QStringLiteral("tripCounterReset"), QStringLiteral("trip.counter-reset")},
            {QStringLiteral("tripExpunge"), QStringLiteral("trip.expunge"), true},
            {QStringLiteral("otaChannel"), QStringLiteral("updates.mdb.channel")},
            {QStringLiteral("otaChannelDbc"), QStringLiteral("updates.dbc.channel")},
            {QStringLiteral("otaMethod"), QStringLiteral("updates.mdb.method")},
            {QStringLiteral("otaMethodDbc"), QStringLiteral("updates.dbc.method")},
            {QStringLiteral("otaCheckInterval"), QStringLiteral("updates.mdb.check-interval")},
            {QStringLiteral("otaCheckIntervalDbc"), QStringLiteral("updates.dbc.check-interval")},
            {QStringLiteral("otaLastCheck"), QStringLiteral("updates.mdb.last-check-time")},
        },
        {}, {}
    };
}

void SettingsStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("dashboard.theme")) {
        if (value != m_theme) { m_theme = value; emit themeChanged(); }
    } else if (variable == QLatin1String("dashboard.mode")) {
        if (value != m_mode) { m_mode = value; emit modeChanged(); }
    } else if (variable == QLatin1String("scooter.developer-mode")) {
        if (value != m_developerMode) { m_developerMode = value; emit developerModeChanged(); }
    } else if (variable == QLatin1String("dashboard.backlight-mode")) {
        if (value != m_backlightMode) { m_backlightMode = value; emit backlightModeChanged(); }
    } else if (variable == QLatin1String("dashboard.show-raw-speed")) {
        if (value != m_showRawSpeed) { m_showRawSpeed = value; emit showRawSpeedChanged(); }
    } else if (variable == QLatin1String("dashboard.speedometer.max-speed")) {
        if (value != m_speedometerMaxSpeed) { m_speedometerMaxSpeed = value; emit speedometerMaxSpeedChanged(); }
    } else if (variable == QLatin1String("dashboard.speedometer.warn-speed")) {
        if (value != m_speedometerWarnSpeed) { m_speedometerWarnSpeed = value; emit speedometerWarnSpeedChanged(); }
    } else if (variable == QLatin1String("dashboard.speedometer.overspeed")) {
        if (value != m_speedometerOverspeed) { m_speedometerOverspeed = value; emit speedometerOverspeedChanged(); }
    } else if (variable == QLatin1String("dashboard.speedometer.base-color")) {
        if (value != m_speedometerBaseColor) { m_speedometerBaseColor = value; emit speedometerBaseColorChanged(); }
    } else if (variable == QLatin1String("dashboard.speedometer.warn-color")) {
        if (value != m_speedometerWarnColor) { m_speedometerWarnColor = value; emit speedometerWarnColorChanged(); }
    } else if (variable == QLatin1String("dashboard.speedometer.overspeed-color")) {
        if (value != m_speedometerOverspeedColor) { m_speedometerOverspeedColor = value; emit speedometerOverspeedColorChanged(); }
    } else if (variable == QLatin1String("dashboard.battery-display-mode")) {
        if (value != m_batteryDisplayMode) { m_batteryDisplayMode = value; emit batteryDisplayModeChanged(); }
    } else if (variable == QLatin1String("dashboard.map.type")) {
        auto v = ScootEnums::parseMapType(value);
        if (v != m_mapType) { m_mapType = v; emit mapTypeChanged(); }
    } else if (variable == QLatin1String("dashboard.map.view-mode")) {
        auto v = ScootEnums::parseMapViewMode(value);
        if (v != m_mapViewMode) { m_mapViewMode = v; emit mapViewModeChanged(); }
    } else if (variable == QLatin1String("dashboard.map.north-oriented")) {
        if (value != m_mapNorthOriented) { m_mapNorthOriented = value; emit mapNorthOrientedChanged(); }
    } else if (variable == QLatin1String("dashboard.map.render-mode")) {
        auto v = ScootEnums::parseMapRenderMode(value);
        if (v != m_mapRenderMode) { m_mapRenderMode = v; emit mapRenderModeChanged(); }
    } else if (variable == QLatin1String("dashboard.valhalla-url")) {
        if (value != m_valhallaUrl) { m_valhallaUrl = value; emit valhallaUrlChanged(); }
    } else if (variable == QLatin1String("dashboard.route-preference")) {
        if (value != m_routePreference) { m_routePreference = value; emit routePreferenceChanged(); }
    } else if (variable == QLatin1String("dashboard.avoid-cobblestone")) {
        if (value != m_avoidCobblestone) { m_avoidCobblestone = value; emit avoidCobblestoneChanged(); }
    } else if (variable == QLatin1String("dashboard.language")) {
        if (value != m_language) { m_language = value; emit languageChanged(); }
    } else if (variable == QLatin1String("dashboard.power-display-mode")) {
        auto v = ScootEnums::parsePowerDisplayMode(value);
        if (v != m_powerDisplayMode) { m_powerDisplayMode = v; emit powerDisplayModeChanged(); }
    } else if (variable == QLatin1String("dashboard.blinker-style")) {
        if (value != m_blinkerStyle) { m_blinkerStyle = value; emit blinkerStyleChanged(); }
    } else if (variable == QLatin1String("scooter.dbc-blinker-led")) {
        if (value != m_dbcBlinkerLed) { m_dbcBlinkerLed = value; emit dbcBlinkerLedChanged(); }
    } else if (variable == QLatin1String("scooter.dual-battery")) {
        if (value != m_dualBattery) { m_dualBattery = value; emit dualBatteryChanged(); }
    } else if (variable == QLatin1String("scooter.horn-when-seatbox-open")) {
        if (value != m_hornWhenSeatboxOpen) { m_hornWhenSeatboxOpen = value; emit hornWhenSeatboxOpenChanged(); }
    } else if (variable == QLatin1String("dashboard.show-gps")) {
        if (value != m_showGps) { m_showGps = value; emit showGpsChanged(); }
    } else if (variable == QLatin1String("dashboard.show-bluetooth")) {
        if (value != m_showBluetooth) { m_showBluetooth = value; emit showBluetoothChanged(); }
    } else if (variable == QLatin1String("dashboard.show-cloud")) {
        if (value != m_showCloud) { m_showCloud = value; emit showCloudChanged(); }
    } else if (variable == QLatin1String("dashboard.show-internet")) {
        if (value != m_showInternet) { m_showInternet = value; emit showInternetChanged(); }
    } else if (variable == QLatin1String("dashboard.show-clock")) {
        if (value != m_showClock) { m_showClock = value; emit showClockChanged(); }
    } else if (variable == QLatin1String("dashboard.show-temperature")) {
        if (value != m_showTemperature) { m_showTemperature = value; emit showTemperatureChanged(); }
    } else if (variable == QLatin1String("dashboard.show-cb-battery")) {
        if (value != m_showCbBattery) { m_showCbBattery = value; emit showCbBatteryChanged(); }
    } else if (variable == QLatin1String("dashboard.show-aux-battery")) {
        if (value != m_showAuxBattery) { m_showAuxBattery = value; emit showAuxBatteryChanged(); }
    } else if (variable == QLatin1String("dashboard.show-road-name")) {
        if (value != m_showRoadName) { m_showRoadName = value; emit showRoadNameChanged(); }
    } else if (variable == QLatin1String("dashboard.show-speed-limit")) {
        if (value != m_showSpeedLimit) { m_showSpeedLimit = value; emit showSpeedLimitChanged(); }
    } else if (variable == QLatin1String("alarm.enabled")) {
        if (value != m_alarmEnabled) { m_alarmEnabled = value; emit alarmEnabledChanged(); }
    } else if (variable == QLatin1String("alarm.honk")) {
        if (value != m_alarmHonk) { m_alarmHonk = value; emit alarmHonkChanged(); }
    } else if (variable == QLatin1String("alarm.duration")) {
        if (value != m_alarmDuration) { m_alarmDuration = value; emit alarmDurationChanged(); }
    } else if (variable == QLatin1String("dashboard.hop-on-combo")) {
        if (value != m_hopOnCombo) { m_hopOnCombo = value; emit hopOnComboChanged(); }
    } else if (variable == QLatin1String("dashboard.maps.check-for-updates")) {
        if (value != m_mapCheckForUpdates) { m_mapCheckForUpdates = value; emit mapCheckForUpdatesChanged(); }
    } else if (variable == QLatin1String("dashboard.maps.auto-download")) {
        if (value != m_mapAutoDownload) { m_mapAutoDownload = value; emit mapAutoDownloadChanged(); }
    } else if (variable == QLatin1String("dashboard.map.traffic-overlay")) {
        if (value != m_mapTrafficOverlay) { m_mapTrafficOverlay = value; emit mapTrafficOverlayChanged(); }
    } else if (variable == QLatin1String("dashboard.milestone-celebrations")) {
        if (value != m_milestoneCelebrations) { m_milestoneCelebrations = value; emit milestoneCelebrationsChanged(); }
    } else if (variable == QLatin1String("dashboard.service-mode-active")) {
        if (value != m_serviceActive) {
            m_serviceActive = value;
            emit serviceActiveChanged();
        }
    } else if (variable == QLatin1String("trip.counter-reset")) {
        if (value != m_tripCounterReset) { m_tripCounterReset = value; emit tripCounterResetChanged(); }
    } else if (variable == QLatin1String("trip.expunge")) {
        const bool available = !value.isEmpty();
        if (available != m_tripExpungeAvailable) {
            m_tripExpungeAvailable = available;
            emit tripExpungeAvailableChanged();
        }
        const QString normalized = isValidTripExpunge(value) ? value : defaultTripExpunge();
        if (normalized != m_tripExpunge) { m_tripExpunge = normalized; emit tripExpungeChanged(); }
    } else if (variable == QLatin1String("updates.mdb.channel")) {
        if (value != m_otaChannel) { m_otaChannel = value; emit otaChannelChanged(); }
    // The DBC half of each pair reuses the MDB signal: it moves the same
    // setting, and every consumer wants to hear about a change on either board.
    } else if (variable == QLatin1String("updates.dbc.channel")) {
        if (value != m_otaChannelDbc) { m_otaChannelDbc = value; emit otaChannelChanged(); }
    } else if (variable == QLatin1String("updates.mdb.method")) {
        if (value != m_otaMethod) { m_otaMethod = value; emit otaMethodChanged(); }
    } else if (variable == QLatin1String("updates.dbc.method")) {
        if (value != m_otaMethodDbc) { m_otaMethodDbc = value; emit otaMethodChanged(); }
    } else if (variable == QLatin1String("updates.mdb.check-interval")) {
        if (value != m_otaCheckInterval) { m_otaCheckInterval = value; emit otaCheckIntervalChanged(); }
    } else if (variable == QLatin1String("updates.dbc.check-interval")) {
        if (value != m_otaCheckIntervalDbc) { m_otaCheckIntervalDbc = value; emit otaCheckIntervalChanged(); }
    } else if (variable == QLatin1String("updates.mdb.last-check-time")) {
        if (value != m_otaLastCheck) { m_otaLastCheck = value; emit otaLastCheckChanged(); }
    }
}

bool SettingsStore::isValidTripExpunge(const QString &value)
{
    if (value == QLatin1String("never"))
        return true;

    const qsizetype colon = value.indexOf(QLatin1Char(':'));
    if (colon <= 0 || colon == value.size() - 1)
        return false;
    const QString policy = value.left(colon);
    const QString operand = value.mid(colon + 1);
    if (policy == QLatin1String("age")) {
        if (operand.endsWith(QLatin1Char('d'))) {
            const QString days = operand.left(operand.size() - 1);
            qint64 count = 0;
            return isCanonicalNonnegativeDecimal(days) && parseNonnegativeInt64(days, count)
                && count >= 1 && count <= 106751;
        }
        return isValidPositiveDuration(operand);
    }
    return (policy == QLatin1String("count") || policy == QLatin1String("size"))
        && isCanonicalNonnegativeDecimal(operand);
}

QString SettingsStore::tripExpungePolicy(const QString &value)
{
    return isValidTripExpunge(value) && value != QLatin1String("never")
        ? value.section(QLatin1Char(':'), 0, 0) : (value == QLatin1String("never") ? value : QString());
}

QString SettingsStore::tripExpungeValue(const QString &value)
{
    return isValidTripExpunge(value) && value != QLatin1String("never")
        ? value.section(QLatin1Char(':'), 1) : QString();
}
