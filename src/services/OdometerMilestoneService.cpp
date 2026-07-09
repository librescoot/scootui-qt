#include "OdometerMilestoneService.h"

#include "stores/EngineStore.h"
#include "stores/VehicleStore.h"
#include "stores/ConnectionStore.h"
#include "stores/SettingsStore.h"
#include "models/Enums.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kIntensityCap = 10;

struct EasterEgg {
    double km;
    const char *tag;
    int intensity;
};

// One-shot easter eggs. Fractional km values that users will roll past
// naturally. Not persisted, so they can repeat across app restarts if the
// user somehow hits them again (e.g. simulator play).
constexpr EasterEgg kEasterEggs[] = {
    {  666.0, "devil",    5 },
    { 1024.0, "power2",   5 },
    { 1234.5, "sequence", 5 },
    { 1337.0, "leet",     6 },
    { 3133.7, "leet_rev", 6 },
    { 8008.5, "boobs",    7 },
    { 9999.9, "rollover", 8 },
};
}  // namespace

OdometerMilestoneService::OdometerMilestoneService(EngineStore *engineStore,
                                                   VehicleStore *vehicleStore,
                                                   ConnectionStore *connectionStore,
                                                   SettingsStore *settingsStore,
                                                   QObject *parent)
    : QObject(parent)
    , m_engineStore(engineStore)
    , m_vehicleStore(vehicleStore)
    , m_connectionStore(connectionStore)
    , m_settingsStore(settingsStore)
{
    m_lastCelebrated = loadLastMilestone();
    // If the rider turns celebrations off mid-ride, drop anything queued for
    // the next park so it doesn't fire later.
    if (m_settingsStore) {
        connect(m_settingsStore, &SettingsStore::milestoneCelebrationsChanged,
                this, [this]() {
                    if (!m_settingsStore->milestoneCelebrations()) m_queue.clear();
                });
    }
    // Easter eggs are unlocked by a gesture on the About screen and
    // persisted so they stay on across restarts until the gesture toggles
    // them off again.
    m_easterEggsEnabled = loadEasterEggsEnabled();
    connect(m_engineStore, &EngineStore::odometerChanged,
            this, &OdometerMilestoneService::onOdometerChanged);
    if (m_vehicleStore) {
        connect(m_vehicleStore, &VehicleStore::stateChanged,
                this, &OdometerMilestoneService::onVehicleStateChanged);
        m_lastVehicleState = m_vehicleStore->state();
    }

    QTimer::singleShot(5000, this, [this]() {
        m_settled = true;
        const double km = m_maxSeenDuringSettle;
        const int milestone = milestoneForKm(km);
        if (milestone > m_lastCelebrated) {
            m_lastCelebrated = milestone;
            saveLastMilestone(m_lastCelebrated);
            qDebug() << "OdometerMilestone: seeded baseline to"
                     << m_lastCelebrated << "km (odo" << km << "km)";
        }
        // Mark any easter eggs already below current odo as seen so we don't
        // retroactively celebrate them on startup.
        for (const auto &egg : kEasterEggs) {
            if (km >= egg.km) m_firedEasterEggs.insert(QString::fromLatin1(egg.tag));
        }
        m_lastOdoKm = km;
    });
}

int OdometerMilestoneService::milestoneForKm(double km)
{
    if (km < 500.0) return 0;
    return static_cast<int>(std::floor(km / 500.0)) * 500;
}

int OdometerMilestoneService::intensityForMilestone(int milestoneKm)
{
    if (milestoneKm <= 0) return 0;
    // 500 → 1, 1000 → 2, 2500 → 5, 5000 → 10 (capped).
    int base = milestoneKm / 500;
    return std::min(base, kIntensityCap);
}

void OdometerMilestoneService::onOdometerChanged()
{
    if (!m_engineStore) return;

    // Floor to 0.1 km to match how odometers tick over (and the status bar
    // display), so the celebration fires the instant the displayed value
    // reaches the milestone, not before.
    const double rawKm = m_engineStore->odometer() / 1000.0;
    const double odoKm = std::floor(rawKm * 10.0) / 10.0;
    if (odoKm <= 0.0) return;

    if (odoKm > m_maxSeenDuringSettle) m_maxSeenDuringSettle = odoKm;
    if (!m_settled) return;

    if (m_connectionStore && !m_connectionStore->hasEverConnected()) return;
    if (m_vehicleStore) {
        const auto st = static_cast<ScootEnums::VehicleState>(m_vehicleStore->state());
        if (st == ScootEnums::VehicleState::Unknown) return;
    }

    const double prevKm = m_lastOdoKm;
    m_lastOdoKm = odoKm;

    // Master off-switch: suppress all output (milestones, toast, easter
    // eggs) but keep the baseline current so flipping it back on later
    // celebrates future crossings, not the ones passed while it was off.
    if (!celebrationsEnabled()) {
        const int m = milestoneForKm(odoKm);
        if (m > m_lastCelebrated) {
            m_lastCelebrated = m;
            saveLastMilestone(m);
        }
        for (const auto &egg : kEasterEggs) {
            if (odoKm >= egg.km) m_firedEasterEggs.insert(QString::fromLatin1(egg.tag));
        }
        return;
    }

    // Easter eggs first — fire on upward crossing of the exact value.
    if (m_easterEggsEnabled)
    for (const auto &egg : kEasterEggs) {
        QString tag = QString::fromLatin1(egg.tag);
        if (m_firedEasterEggs.contains(tag)) continue;
        if (prevKm >= 0.0 && prevKm < egg.km && odoKm >= egg.km) {
            m_firedEasterEggs.insert(tag);
            qDebug() << "OdometerMilestone: easter egg" << tag << "at" << egg.km << "km";
            enqueueAndCross(egg.km, egg.intensity, tag);
            return;  // one crossing at a time
        }
    }

    const int milestone = milestoneForKm(odoKm);
    if (milestone <= m_lastCelebrated) return;

    m_lastCelebrated = milestone;
    saveLastMilestone(milestone);

    const int intensity = intensityForMilestone(milestone);
    qDebug() << "OdometerMilestone: reached" << milestone << "km (intensity"
             << intensity << ")";
    enqueueAndCross(static_cast<double>(milestone), intensity, QString());
}

void OdometerMilestoneService::enqueueAndCross(double km, int intensity, const QString &tag)
{
    emit milestoneCrossed(km, intensity, tag);
    m_queue.append({km, intensity, tag});

    // If the scooter is already parked when a milestone is crossed (e.g.
    // simulator scrub, manual odometer edit), kick off the celebration
    // right away — otherwise it would sit in the queue forever.
    if (!m_celebrating && m_vehicleStore) {
        const auto st = static_cast<ScootEnums::VehicleState>(m_vehicleStore->state());
        if (st == ScootEnums::VehicleState::Parked) {
            startNextCelebration();
        }
    }
}

void OdometerMilestoneService::onVehicleStateChanged()
{
    if (!m_vehicleStore) return;
    const int prev = m_lastVehicleState;
    const int now = m_vehicleStore->state();
    m_lastVehicleState = now;

    const auto nowState = static_cast<ScootEnums::VehicleState>(now);
    const bool prevWasParked = prev == static_cast<int>(ScootEnums::VehicleState::Parked);
    if (nowState == ScootEnums::VehicleState::Parked && !prevWasParked) {
        if (!m_celebrating && !m_queue.isEmpty()) {
            startNextCelebration();
        }
    }
}

void OdometerMilestoneService::startNextCelebration()
{
    if (m_queue.isEmpty()) {
        m_celebrating = false;
        return;
    }
    const Pending next = m_queue.takeFirst();
    m_celebrating = true;
    qDebug() << "OdometerMilestone: celebrate" << next.km << "km tag" << next.tag;
    emit milestoneCelebrate(next.km, next.intensity, next.tag);
}

void OdometerMilestoneService::advanceCelebration()
{
    if (m_queue.isEmpty()) {
        m_celebrating = false;
        return;
    }
    startNextCelebration();
}

bool OdometerMilestoneService::celebrationsEnabled() const
{
    return !m_settingsStore || m_settingsStore->milestoneCelebrations();
}

void OdometerMilestoneService::setEasterEggsEnabled(bool enabled)
{
    if (enabled == m_easterEggsEnabled) return;
    m_easterEggsEnabled = enabled;
    saveEasterEggsEnabled(enabled);
    emit easterEggsEnabledChanged();
}

QString OdometerMilestoneService::easterEggsPath() const
{
#ifdef Q_OS_LINUX
    return QStringLiteral("/data/scootui/easter-eggs");
#else
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dir + QStringLiteral("/easter-eggs");
#endif
}

bool OdometerMilestoneService::loadEasterEggsEnabled() const
{
    QFile f(easterEggsPath());
    if (!f.exists()) return false;
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    return f.readAll().trimmed() == "1";
}

void OdometerMilestoneService::saveEasterEggsEnabled(bool enabled)
{
    const QString path = easterEggsPath();
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return;
    f.write(enabled ? "1\n" : "0\n");
}

QString OdometerMilestoneService::persistPath() const
{
#ifdef Q_OS_LINUX
    return QStringLiteral("/data/scootui/milestone");
#else
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dir + QStringLiteral("/milestone");
#endif
}

int OdometerMilestoneService::loadLastMilestone() const
{
    const QString path = persistPath();
    QFile f(path);
    if (!f.exists()) return -1;
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "OdometerMilestone: failed to read" << path;
        return -1;
    }
    QByteArray data = f.readAll().trimmed();
    bool ok = false;
    int v = data.toInt(&ok);
    return ok ? v : -1;
}

void OdometerMilestoneService::saveLastMilestone(int km)
{
    const QString path = persistPath();
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qWarning() << "OdometerMilestone: failed to write" << path;
        return;
    }
    QTextStream out(&f);
    out << km << '\n';
}
