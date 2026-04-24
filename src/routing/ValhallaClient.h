#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <QDateTime>
#include <QElapsedTimer>
#include "RouteModels.h"

class ValhallaClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool healthy READ isHealthy NOTIFY healthChanged)

public:
    enum class Reason {
        Initial,         // user-initiated, first post-boot route
        Destination,     // user-initiated destination change
        Reroute,         // automatic, off-route
        Recovery,        // automatic, post-error GPS edge
        LanguageChange   // user-initiated, re-route on locale change
    };
    Q_ENUM(Reason)

    enum class RejectionCause {
        RateLimited,     // 429 backoff active
        Cooldown,        // 15s reroute cooldown
        TooSoon,         // per-user rate-floor
        Unhealthy        // health gate says server not ready
    };
    Q_ENUM(RejectionCause)

    explicit ValhallaClient(QObject *parent = nullptr);

    void setEndpoint(const QString &url);
    QString endpoint() const { return m_endpoint; }
    void setLanguage(const QString &lang);

    bool isHealthy() const { return m_isHealthy; }

    // Build date of the currently-loaded tileset, as reported by /status.
    // Invalid QDateTime if /status has never succeeded or the field was
    // missing. Updated on every successful health probe.
    QDateTime tilesetLastModified() const { return m_tilesetLastModified; }

    // Single entry point. Coalesces rapid callers via DebounceIntervalMs; latest
    // (from, to, reason) wins. Governance applied at dispatch time.
    void requestRoute(const LatLng &from, const LatLng &to, Reason reason);

    // Cancel any pending + in-flight.
    void cancelPending();

    // Force an immediate health probe regardless of cache state.
    void checkStatus();

    static constexpr int DebounceIntervalMs        = 200;
    static constexpr int MinAutoInterRequestMs     = 1100;   // valhalla1.osm.de: 1/sec/user
    static constexpr int RerouteCooldownMs         = 15000;
    static constexpr int RateLimitBackoffMinMs     = 30000;
    static constexpr int RateLimitBackoffMaxMs     = 120000;
    static constexpr int HealthProbeBackoffMinMs   = 2000;
    static constexpr int HealthProbeBackoffMaxMs   = 10000;
    static constexpr int HealthProbeTimeoutMs      = 3000;
    static constexpr int HealthCacheMs             = 60000;
    // A user-initiated request that stays queued waiting for the first
    // healthy probe gives up after this long and surfaces an error to the
    // user instead of hanging on "Calculating" indefinitely.
    static constexpr int UserRequestTimeoutMs      = 20000;

signals:
    void routeCalculated(const Route &route);
    void routeError(const QString &error);
    void rateLimited();
    void statusChecked(bool available);
    void requestRejected(ValhallaClient::Reason reason, ValhallaClient::RejectionCause cause);
    void healthChanged();
    void tilesetLastModifiedChanged();

private:
    enum class DispatchResult {
        OK,
        NotYetHealthy,
        Rejected
    };

    static bool isUserReason(Reason r) {
        return r == Reason::Initial || r == Reason::Destination || r == Reason::LanguageChange;
    }

    void dispatchPending();
    DispatchResult canDispatch(Reason reason, RejectionCause &cause) const;
    void sendRouteRequest(const LatLng &from, const LatLng &to);
    void handleRouteReply(QNetworkReply *reply);

    void scheduleHealthProbe(int delayMs);
    void runHealthProbe();
    void handleHealthReply(QNetworkReply *reply, bool forced);

    bool rateLimitBackoffActive() const;
    bool rerouteCooldownActive() const;
    bool rateFloorActive() const;

    QNetworkAccessManager m_nam;
    QString m_endpoint;
    QString m_language = QStringLiteral("en-US");

    // Debounce: latest pending request, dispatched when m_debounce fires
    QTimer m_debounce;
    LatLng m_pendingFrom;
    LatLng m_pendingTo;
    Reason m_pendingReason = Reason::Initial;
    bool m_hasPending = false;

    QPointer<QNetworkReply> m_activeReply;

    // 429 backoff
    int m_rateLimitBackoffMs = 0;
    QElapsedTimer m_sinceRateLimit;
    bool m_rateLimitArmed = false;  // true once m_sinceRateLimit has been started

    // Reroute cooldown
    QElapsedTimer m_sinceLastReroute;
    bool m_hasRerouted = false;

    // Per-user rate-floor for auto dispatches
    QElapsedTimer m_sinceLastDispatch;
    bool m_firstAutoDispatch = true;

    // Health state
    bool m_isHealthy = false;
    bool m_hasBeenHealthy = false;
    bool m_probeInFlight = false;
    int m_probeBackoffMs = HealthProbeBackoffMinMs;
    QTimer m_healthTimer;
    QPointer<QNetworkReply> m_healthReply;
    QDateTime m_tilesetLastModified;

    // Single-shot deadline for a user request stuck in NotYetHealthy.
    QTimer m_userRequestDeadline;
};
