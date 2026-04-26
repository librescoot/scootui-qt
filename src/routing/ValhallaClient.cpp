#include "ValhallaClient.h"
#include "RouteHelpers.h"

#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSslConfiguration>
#include <QDebug>

ValhallaClient::ValhallaClient(QObject *parent)
    : QObject(parent)
    , m_endpoint(QStringLiteral("http://127.0.0.1:8002/"))
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(DebounceIntervalMs);
    connect(&m_debounce, &QTimer::timeout, this, &ValhallaClient::dispatchPending);

    m_healthTimer.setSingleShot(true);
    connect(&m_healthTimer, &QTimer::timeout, this, &ValhallaClient::runHealthProbe);

    m_userRequestDeadline.setSingleShot(true);
    m_userRequestDeadline.setInterval(UserRequestTimeoutMs);
    connect(&m_userRequestDeadline, &QTimer::timeout, this, [this]() {
        // Race: probe may have succeeded and dispatched between timer fire
        // and handler run. Only surface the error if we're still queued.
        if (!m_hasPending || m_hasBeenHealthy || !isUserReason(m_pendingReason))
            return;
        Reason reason = m_pendingReason;
        m_hasPending = false;
        qDebug() << "ValhallaClient: user request timed out waiting for healthy probe";
        emit requestRejected(reason, RejectionCause::Unhealthy);
    });

    m_sinceLastReroute.start();
    m_sinceLastDispatch.start();

    // Kick off the first health probe so the gate can open before any user
    // request arrives.
    scheduleHealthProbe(0);
}

void ValhallaClient::setEndpoint(const QString &url)
{
    m_endpoint = url;
    if (!m_endpoint.endsWith(QLatin1Char('/')))
        m_endpoint.append(QLatin1Char('/'));
}

void ValhallaClient::setLanguage(const QString &lang)
{
    m_language = lang;
    qDebug() << "ValhallaClient: language set to" << m_language;
}

bool ValhallaClient::rateLimitBackoffActive() const
{
    if (m_rateLimitBackoffMs <= 0 || !m_rateLimitArmed)
        return false;
    return m_sinceRateLimit.elapsed() < m_rateLimitBackoffMs;
}

bool ValhallaClient::rerouteCooldownActive() const
{
    if (!m_hasRerouted)
        return false;
    return m_sinceLastReroute.elapsed() < RerouteCooldownMs;
}

bool ValhallaClient::rateFloorActive() const
{
    if (m_firstAutoDispatch)
        return false;
    return m_sinceLastDispatch.elapsed() < MinAutoInterRequestMs;
}

void ValhallaClient::requestRoute(const LatLng &from, const LatLng &to, Reason reason)
{
    m_pendingFrom = from;
    m_pendingTo = to;
    m_pendingReason = reason;
    m_hasPending = true;
    m_debounce.start();
}

void ValhallaClient::cancelPending()
{
    m_hasPending = false;
    m_debounce.stop();
    m_userRequestDeadline.stop();
    if (m_activeReply) {
        m_activeReply->disconnect(this);
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply.clear();
    }
    abortActiveTrace();
}

void ValhallaClient::abortActiveTrace()
{
    if (m_activeTraceReply) {
        m_activeTraceReply->disconnect(this);
        m_activeTraceReply->abort();
        m_activeTraceReply->deleteLater();
        m_activeTraceReply.clear();
    }
}

ValhallaClient::DispatchResult ValhallaClient::canDispatch(Reason reason, RejectionCause &cause) const
{
    if (isUserReason(reason)) {
        if (!m_hasBeenHealthy)
            return DispatchResult::NotYetHealthy;
        if (rateLimitBackoffActive()) {
            cause = RejectionCause::RateLimited;
            return DispatchResult::Rejected;
        }
        return DispatchResult::OK;
    }

    // Auto reason
    if (!m_isHealthy) {
        cause = RejectionCause::Unhealthy;
        return DispatchResult::Rejected;
    }
    if (rateLimitBackoffActive()) {
        cause = RejectionCause::RateLimited;
        return DispatchResult::Rejected;
    }
    if (reason == Reason::Reroute && rerouteCooldownActive()) {
        cause = RejectionCause::Cooldown;
        return DispatchResult::Rejected;
    }
    if (rateFloorActive()) {
        cause = RejectionCause::TooSoon;
        return DispatchResult::Rejected;
    }
    return DispatchResult::OK;
}

void ValhallaClient::dispatchPending()
{
    if (!m_hasPending)
        return;

    RejectionCause cause = RejectionCause::RateLimited;
    DispatchResult result = canDispatch(m_pendingReason, cause);

    switch (result) {
    case DispatchResult::OK: {
        LatLng from = m_pendingFrom;
        LatLng to = m_pendingTo;
        Reason reason = m_pendingReason;
        m_hasPending = false;
        m_userRequestDeadline.stop();

        sendRouteRequest(from, to);

        m_sinceLastDispatch.restart();
        m_firstAutoDispatch = false;
        if (reason == Reason::Reroute) {
            m_sinceLastReroute.restart();
            m_hasRerouted = true;
        }
        break;
    }
    case DispatchResult::NotYetHealthy:
        // Keep pending; when the health probe flips to healthy it will
        // call dispatchPending() again. Arm the deadline so we don't
        // hang forever if the server is genuinely down.
        qDebug() << "ValhallaClient: deferring user request until first healthy probe";
        if (!m_userRequestDeadline.isActive())
            m_userRequestDeadline.start();
        break;
    case DispatchResult::Rejected: {
        qDebug() << "ValhallaClient: rejecting"
                 << static_cast<int>(m_pendingReason)
                 << "cause" << static_cast<int>(cause);
        Reason rejectedReason = m_pendingReason;
        m_hasPending = false;
        m_userRequestDeadline.stop();
        emit requestRejected(rejectedReason, cause);
        break;
    }
    }
}

void ValhallaClient::sendRouteRequest(const LatLng &from, const LatLng &to)
{
    // Abort any in-flight reply before issuing a new request so its result
    // can't race ahead of the one we're about to dispatch
    if (m_activeReply) {
        m_activeReply->disconnect(this);
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply.clear();
    }
    // Drop any in-flight trace_attributes too — it's about to be tied to a
    // stale route. The next routeCalculated will trigger a fresh trace.
    abortActiveTrace();

    QJsonObject request;
    QJsonArray locations;
    locations.append(QJsonObject{{QStringLiteral("lat"), from.latitude},
                                  {QStringLiteral("lon"), from.longitude},
                                  {QStringLiteral("radius"), 150}});
    locations.append(QJsonObject{{QStringLiteral("lat"), to.latitude},
                                  {QStringLiteral("lon"), to.longitude},
                                  {QStringLiteral("radius"), 150}});
    request[QStringLiteral("locations")] = locations;
    request[QStringLiteral("costing")] = QStringLiteral("motor_scooter");
    request[QStringLiteral("units")] = QStringLiteral("kilometers");
    request[QStringLiteral("language")] = m_language;
    request[QStringLiteral("shape_format")] = QStringLiteral("polyline6");
    QJsonObject dirOpts;
    dirOpts[QStringLiteral("units")] = QStringLiteral("kilometers");
    dirOpts[QStringLiteral("language")] = m_language;
    request[QStringLiteral("directions_options")] = dirOpts;

    QNetworkRequest req(QUrl(m_endpoint + QStringLiteral("route")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(30000);

    // On embedded systems the clock may be wrong at boot, causing
    // "certificate not yet valid" errors. Disable peer verification
    // for the routing request (localhost or trusted public endpoint).
    QSslConfiguration ssl = req.sslConfiguration();
    ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
    req.setSslConfiguration(ssl);

    QByteArray body = QJsonDocument(request).toJson(QJsonDocument::Compact);
    qDebug() << "ValhallaClient: POST" << req.url().toString() << body;
    auto *reply = m_nam.post(req, body);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleRouteReply(reply);
    });
}

void ValhallaClient::checkStatus()
{
    // Force an immediate probe, cancelling any scheduled one.
    m_healthTimer.stop();
    runHealthProbe();
}

void ValhallaClient::scheduleHealthProbe(int delayMs)
{
    if (m_probeInFlight || m_healthTimer.isActive())
        return;
    m_healthTimer.start(delayMs);
}

void ValhallaClient::runHealthProbe()
{
    if (m_probeInFlight)
        return;

    QNetworkRequest req(QUrl(m_endpoint + QStringLiteral("status")));
    req.setTransferTimeout(HealthProbeTimeoutMs);
    QSslConfiguration ssl = req.sslConfiguration();
    ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
    req.setSslConfiguration(ssl);

    m_probeInFlight = true;
    auto *reply = m_nam.get(req);
    m_healthReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleHealthReply(reply, false);
    });
}

void ValhallaClient::handleHealthReply(QNetworkReply *reply, bool /*forced*/)
{
    reply->deleteLater();
    if (m_healthReply == reply)
        m_healthReply.clear();
    m_probeInFlight = false;

    bool ok = (reply->error() == QNetworkReply::NoError);
    emit statusChecked(ok);

    if (ok) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isObject()) {
            auto v = doc.object().value(QStringLiteral("tileset_last_modified"));
            qint64 ts = v.isDouble() ? static_cast<qint64>(v.toDouble()) : v.toVariant().toLongLong();
            if (ts > 0) {
                QDateTime dt = QDateTime::fromSecsSinceEpoch(ts, Qt::UTC);
                if (dt != m_tilesetLastModified) {
                    m_tilesetLastModified = dt;
                    emit tilesetLastModifiedChanged();
                }
            }
        }
    }

    bool wasHealthy = m_isHealthy;
    if (ok) {
        m_probeBackoffMs = HealthProbeBackoffMinMs;
        m_isHealthy = true;
        bool firstHealthy = !m_hasBeenHealthy;
        m_hasBeenHealthy = true;
        if (wasHealthy != m_isHealthy)
            emit healthChanged();
        // Keep-alive poll to keep the health signal fresh.
        scheduleHealthProbe(HealthCacheMs);
        // Flush any request that was queued waiting for the first healthy probe.
        if (firstHealthy && m_hasPending)
            dispatchPending();
    } else {
        m_isHealthy = false;
        if (wasHealthy != m_isHealthy)
            emit healthChanged();
        m_probeBackoffMs = qMin(m_probeBackoffMs * 2, HealthProbeBackoffMaxMs);
        scheduleHealthProbe(m_probeBackoffMs);
    }
}

void ValhallaClient::handleRouteReply(QNetworkReply *reply)
{
    reply->deleteLater();
    if (m_activeReply == reply)
        m_activeReply.clear();

    if (reply->error() != QNetworkReply::NoError) {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString msg;
        bool connectionError = false;
        if (reply->error() == QNetworkReply::OperationCanceledError ||
            reply->error() == QNetworkReply::TimeoutError) {
            msg = QStringLiteral("Connection to routing server timed out");
            connectionError = true;
        } else if (reply->error() == QNetworkReply::ConnectionRefusedError ||
                   reply->error() == QNetworkReply::HostNotFoundError) {
            msg = QStringLiteral("Cannot connect to routing server");
            connectionError = true;
        } else if (status == 400) {
            msg = QStringLiteral("Invalid route request. Destination may be unreachable.");
        } else if (status == 429) {
            m_rateLimitBackoffMs = qMin(
                qMax(m_rateLimitBackoffMs * 2, RateLimitBackoffMinMs),
                RateLimitBackoffMaxMs);
            m_sinceRateLimit.restart();
            m_rateLimitArmed = true;
            qDebug() << "ValhallaClient: rate limited, backoff now" << m_rateLimitBackoffMs << "ms";
            emit rateLimited();
            msg = QStringLiteral("Too many routing requests. Please wait a moment.");
        } else if (status >= 500) {
            msg = QStringLiteral("Routing server error. Please try again later.");
        } else {
            msg = reply->errorString();
        }

        if (connectionError) {
            // Treat as unhealthy and probe immediately so the gate reopens
            // as soon as the server is reachable again.
            bool wasHealthy = m_isHealthy;
            m_isHealthy = false;
            if (wasHealthy)
                emit healthChanged();
            scheduleHealthProbe(0);
        }

        emit routeError(msg);
        return;
    }

    QByteArray data = reply->readAll();
    Route route = RouteHelpers::parseRouteResponse(data);
    if (route.isValid()) {
        // Successful response: clear backoffs.
        m_rateLimitBackoffMs = 0;
        m_rateLimitArmed = false;
        m_probeBackoffMs = HealthProbeBackoffMinMs;
        emit routeCalculated(route);
        // Fire the lower-priority follow-up to enrich the route with
        // per-edge speed_limit / tunnel / bridge / names. Async; if it
        // fails or is rejected, RoadInfoService falls through to the
        // tile path with Layer 1's route-name bias.
        requestTraceAttributes(route.waypoints);
    } else {
        emit routeError(QStringLiteral("Failed to parse route response"));
    }
}

void ValhallaClient::requestTraceAttributes(const QList<LatLng> &shape)
{
    if (shape.size() < 2)
        return;
    // Lower-priority lane: yield to anything route-related. A queued route
    // request means a fresh /route is moments away; an in-flight route reply
    // means we'd race the route's own emission. Skip silently in either case.
    if (m_hasPending || m_activeReply)
        return;
    // Same backoff/health gates as auto routes — never pile on after a 429,
    // never hit a server we know is down.
    if (rateLimitBackoffActive() || !m_isHealthy)
        return;

    abortActiveTrace();

    QJsonObject req;
    QJsonArray shapeArr;
    for (const auto &p : shape) {
        shapeArr.append(QJsonObject{
            {QStringLiteral("lat"), p.latitude},
            {QStringLiteral("lon"), p.longitude}
        });
    }
    req[QStringLiteral("shape")] = shapeArr;
    req[QStringLiteral("costing")] = QStringLiteral("motor_scooter");
    // edge_walk treats the shape as a known polyline along graph edges, much
    // cheaper than map_snap. Safe here because the polyline came from
    // Valhalla's own /route response moments ago.
    req[QStringLiteral("shape_match")] = QStringLiteral("edge_walk");
    QJsonObject filters;
    filters[QStringLiteral("action")] = QStringLiteral("include");
    QJsonArray attrs;
    attrs.append(QStringLiteral("edge.names"));
    attrs.append(QStringLiteral("edge.speed_limit"));
    attrs.append(QStringLiteral("edge.tunnel"));
    attrs.append(QStringLiteral("edge.bridge"));
    attrs.append(QStringLiteral("edge.road_class"));
    attrs.append(QStringLiteral("edge.begin_shape_index"));
    attrs.append(QStringLiteral("edge.end_shape_index"));
    filters[QStringLiteral("attributes")] = attrs;
    req[QStringLiteral("filters")] = filters;

    QNetworkRequest httpReq(QUrl(m_endpoint + QStringLiteral("trace_attributes")));
    httpReq.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    httpReq.setTransferTimeout(15000);
    QSslConfiguration ssl = httpReq.sslConfiguration();
    ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
    httpReq.setSslConfiguration(ssl);

    QByteArray body = QJsonDocument(req).toJson(QJsonDocument::Compact);
    int segmentCount = shape.size() - 1;
    auto *reply = m_nam.post(httpReq, body);
    m_activeTraceReply = reply;
    // Counts toward the per-user rate floor so a subsequent auto-reroute
    // doesn't pile straight on top of the trace request.
    m_sinceLastDispatch.restart();
    m_firstAutoDispatch = false;
    connect(reply, &QNetworkReply::finished, this, [this, reply, segmentCount]() {
        handleTraceAttributesReply(reply, segmentCount);
    });
}

void ValhallaClient::handleTraceAttributesReply(QNetworkReply *reply, int segmentCount)
{
    reply->deleteLater();
    if (m_activeTraceReply == reply)
        m_activeTraceReply.clear();

    if (reply->error() != QNetworkReply::NoError) {
        // Silent failure — no toast, no retry. Route already shipped without
        // shapeAttrs and the consumer falls through to the tile path.
        qDebug() << "ValhallaClient: trace_attributes failed:"
                 << reply->errorString();
        return;
    }

    QByteArray data = reply->readAll();
    QList<EdgeAttrs> attrs =
        RouteHelpers::parseTraceAttributesResponse(data, segmentCount);
    if (attrs.isEmpty()) {
        qDebug() << "ValhallaClient: trace_attributes returned no usable data";
        return;
    }
    emit routeAttributesReady(attrs);
}
