#include "NotificationService.h"

#include "services/AttentionPolicy.h"
#include "stores/VehicleStore.h"
#include "models/Enums.h"

#include <algorithm>

NotificationService::NotificationService(bool simulatorInjectionEnabled, QObject *parent)
    : QObject(parent), m_simulatorInjectionEnabled(simulatorInjectionEnabled)
{
    m_clock.start();
    m_expiryTimer.setInterval(250);
    connect(&m_expiryTimer, &QTimer::timeout, this, &NotificationService::expireEvents);
    m_expiryTimer.start();
    refreshPresentation();
}

QVariantMap NotificationService::makeEntry(const QString &id, const QString &source,
                                            const QString &title, const QString &body,
                                            int priority, const QString &kind) const
{
    QVariantMap entry;
    entry[QStringLiteral("id")] = id;
    entry[QStringLiteral("source")] = source;
    entry[QStringLiteral("title")] = title;
    entry[QStringLiteral("body")] = body;
    entry[QStringLiteral("priority")] = priority;
    entry[QStringLiteral("kind")] = kind;
    entry[QStringLiteral("revision")] = 1;
    return entry;
}

QVariantList NotificationService::active() const
{
    QVariantList result;
    for (const auto &entry : m_conditions)
        result.append(entry);
    return result;
}

QVariantList NotificationService::history() const
{
    QVariantList result;
    for (const auto &entry : m_history)
        result.append(entry);
    return result;
}

void NotificationService::setSurface(const QString &surface)
{
    const QString normalized = surface == QLatin1String("map") ? surface : QStringLiteral("cluster");
    if (normalized == m_surface)
        return;
    m_surface = normalized;
    emit surfaceChanged();
    refreshPresentation();
}

void NotificationService::setNavigationPayload(const QVariantMap &navigation)
{
    if (navigation == m_navigation)
        return;
    m_navigation = navigation;
    refreshPresentation();
}

void NotificationService::setVehicleStore(VehicleStore *vehicle)
{
    if (m_vehicleStore == vehicle)
        return;
    m_vehicleStore = vehicle;
    if (vehicle) {
        connect(vehicle, &VehicleStore::stateChanged, this, &NotificationService::onVehicleStateChanged);
        onVehicleStateChanged();
    }
}

void NotificationService::onVehicleStateChanged()
{
    if (!m_vehicleStore)
        return;
    const auto state = static_cast<ScootEnums::VehicleState>(m_vehicleStore->state());
    const bool riding = state == ScootEnums::VehicleState::ReadyToDrive;
    if (riding == m_riding)
        return;
    m_riding = riding;
    refreshPresentation();
}

QString NotificationService::publishCondition(const QString &id, const QString &source,
                                               const QString &title, const QString &body,
                                               int priority, const QString &kind,
                                               const QString &icon)
{
    if (id.isEmpty())
        return {};
    QVariantMap entry = makeEntry(id, source, title, body, qBound(0, priority, 4), kind);
    if (!icon.isEmpty())
        entry[QStringLiteral("icon")] = icon;
    const auto old = m_conditions.value(id);
    const int revision = old.value(QStringLiteral("revision"), 0).toInt() + 1;
    entry[QStringLiteral("revision")] = revision;
    entry[QStringLiteral("order")] = old.value(QStringLiteral("order"), m_nextOrder++);
    m_conditions.insert(id, entry);
    if (old != entry) {
        emit activeChanged();
        refreshPresentation();
    }
    return id;
}

void NotificationService::resolveCondition(const QString &id)
{
    if (!m_conditions.remove(id))
        return;
    emit activeChanged();
    refreshPresentation();
}

QString NotificationService::publishEvent(const QString &id, const QString &source,
                                           const QString &title, const QString &body,
                                           int priority, const QString &kind, int lifetimeMs)
{
    if (id.isEmpty())
        return {};
    const qint64 now = m_clock.elapsed();
    for (auto &existing : m_events) {
        if (existing.value(QStringLiteral("id")) == id) {
            existing[QStringLiteral("title")] = title;
            existing[QStringLiteral("body")] = body;
            existing[QStringLiteral("updatedAt")] = now;
            existing[QStringLiteral("validUntil")] = now + qMax(1, lifetimeMs);
            refreshPresentation();
            return id;
        }
    }
    QVariantMap entry = makeEntry(id, source, title, body, qBound(0, priority, 4), kind);
    entry[QStringLiteral("createdAt")] = now;
    entry[QStringLiteral("updatedAt")] = now;
    entry[QStringLiteral("order")] = m_nextOrder++;
    entry[QStringLiteral("validUntil")] = now + qMax(1, lifetimeMs);
    m_events.append(entry);
    while (m_events.size() > 20) {
        const auto nowEntry = m_clock.elapsed();
        auto victim = std::min_element(m_events.begin(), m_events.end(), [nowEntry](
            const QVariantMap &a, const QVariantMap &b) {
            const bool aStale = a.value(QStringLiteral("validUntil")).toLongLong() <= nowEntry;
            const bool bStale = b.value(QStringLiteral("validUntil")).toLongLong() <= nowEntry;
            if (aStale != bStale)
                return aStale;
            const int aPriority = a.value(QStringLiteral("priority"), 4).toInt();
            const int bPriority = b.value(QStringLiteral("priority"), 4).toInt();
            if (aPriority != bPriority)
                return aPriority > bPriority;
            return a.value(QStringLiteral("order"), 0).toULongLong()
                < b.value(QStringLiteral("order"), 0).toULongLong();
        });
        if (victim == m_events.end())
            break;
        m_events.erase(victim);
    }
    refreshPresentation();
    if (m_presentation.value(QStringLiteral("main")).toMap()
            .value(QStringLiteral("id")).toString() == id)
        emit eventPresented(kind);
    return id;
}

void NotificationService::clearEvent(const QString &id)
{
    const auto oldSize = m_events.size();
    m_events.erase(std::remove_if(m_events.begin(), m_events.end(), [&id](const QVariantMap &entry) {
        return entry.value(QStringLiteral("id")) == id;
    }), m_events.end());
    if (m_events.size() != oldSize)
        refreshPresentation();
}

void NotificationService::expireEvents()
{
    const qint64 now = m_clock.elapsed();
    bool changed = false;
    QList<QVariantMap> kept;
    for (const auto &entry : std::as_const(m_events)) {
        if (entry.value(QStringLiteral("validUntil")).toLongLong() > now)
            kept.append(entry);
        else {
            QVariantMap historyEntry = entry;
            historyEntry.remove(QStringLiteral("validUntil"));
            m_history.prepend(historyEntry);
            changed = true;
        }
    }
    if (!changed)
        return;
    m_events = kept;
    while (m_history.size() > 50)
        m_history.removeLast();
    emit historyChanged();
    refreshPresentation();
}

void NotificationService::setMapUpdateAvailable(bool available, const QString &title)
{
    const bool changed = available != m_mapUpdateAvailable;
    if (!available) {
        if (!changed)
            return;
        m_mapUpdateAvailable = false;
        resolveCondition(QStringLiteral("map-update"));
        emit mapUpdateAvailableChanged();
        return;
    }

    m_mapUpdateAvailable = true;
    const QString displayTitle = title.isEmpty()
        ? m_conditions.value(QStringLiteral("map-update"))
              .value(QStringLiteral("title"), QStringLiteral("Map update")).toString()
        : title;
    publishCondition(QStringLiteral("map-update"), QStringLiteral("maps"),
                     displayTitle, {}, 3, QStringLiteral("info"));
    if (changed)
        emit mapUpdateAvailableChanged();
}

void NotificationService::setCoverageWarning(bool warning)
{
    if (warning == m_coverageWarning)
        return;
    m_coverageWarning = warning;
    if (warning)
        publishCondition(QStringLiteral("map-coverage"), QStringLiteral("map"),
                         QStringLiteral("No map for current location"),
                         QStringLiteral("Map coverage is unavailable here"), 2, QStringLiteral("warning"));
    else
        resolveCondition(QStringLiteral("map-coverage"));
    emit coverageWarningChanged();
}

void NotificationService::refreshPresentation()
{
    QList<QVariantMap> conditions;
    for (auto condition : m_conditions) {
        if (condition.value(QStringLiteral("source")) == QLatin1String("map"))
            condition[QStringLiteral("surfaceEligible")] = m_surface == QLatin1String("map");
        conditions.append(condition);
    }
    const QVariantMap previousMain = m_presentation.value(QStringLiteral("main")).toMap();
    const QString previousMainId = previousMain.value(QStringLiteral("id")).toString();
    const int previousMainPriority = previousMain.value(QStringLiteral("priority"), 4).toInt();
    const AttentionSelection selection = AttentionPolicy::select(
        conditions, m_events, m_navigation, m_riding, m_clock.elapsed());
    QVariantMap presentation;
    presentation[QStringLiteral("main")] = selection.main;
    presentation[QStringLiteral("companion")] = selection.companion;
    presentation[QStringLiteral("criticalCount")] = selection.criticalCount;
    presentation[QStringLiteral("height")] = selection.height;
    presentation[QStringLiteral("layout")] = selection.height >= 96 ? QStringLiteral("expanded")
                                                                      : selection.height ? QStringLiteral("compact")
                                                                                        : QStringLiteral("idle");
    if (presentation == m_presentation)
        return;
    m_presentation = presentation;
    emit presentationChanged();

    const QVariantMap main = presentation.value(QStringLiteral("main")).toMap();
    const QString currentMainId = main.value(QStringLiteral("id")).toString();
    const int currentMainPriority = main.value(QStringLiteral("priority"), 4).toInt();
    const bool becameMain = currentMainId != previousMainId;
    const bool escalated = currentMainId == previousMainId
        && currentMainPriority < previousMainPriority;
    if (!currentMainId.isEmpty() && (becameMain || escalated)
        && m_conditions.contains(currentMainId)
        && currentMainId != QLatin1String("map-update"))
        emit conditionPresented(main.value(QStringLiteral("kind")).toString());
}

void NotificationService::simulateWarning(const QString &title, const QString &body)
{
    if (m_simulatorInjectionEnabled)
        publishEvent(QStringLiteral("sim-warning"), QStringLiteral("simulator"), title, body, 2,
                     QStringLiteral("warning"));
}

void NotificationService::simulateError(const QString &title, const QString &body)
{
    if (m_simulatorInjectionEnabled)
        publishEvent(QStringLiteral("sim-error"), QStringLiteral("simulator"), title, body, 2,
                     QStringLiteral("error"), 5000);
}

void NotificationService::simulateCritical(const QString &title, const QString &body)
{
    if (m_simulatorInjectionEnabled)
        publishCondition(QStringLiteral("sim-critical"), QStringLiteral("simulator"), title, body, 0,
                         QStringLiteral("critical"));
}

void NotificationService::clearSimulatorEntries()
{
    if (!m_simulatorInjectionEnabled)
        return;
    resolveCondition(QStringLiteral("sim-critical"));
    clearEvent(QStringLiteral("sim-warning"));
    clearEvent(QStringLiteral("sim-error"));
}
