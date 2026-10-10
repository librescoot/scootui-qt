#include "repositories/InMemoryMdbRepository.h"
#include "services/MapStyleComposer.h"
#include "services/RideGroupService.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

class MapProvider : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool isReady READ yes CONSTANT)
  Q_PROPERTY(bool routeOverviewActive READ no CONSTANT)
  Q_PROPERTY(bool debugZoomEnabled READ no CONSTANT)
  Q_PROPERTY(double mapLatitude READ latitude CONSTANT)
  Q_PROPERTY(double mapLongitude READ longitude CONSTANT)
  Q_PROPERTY(double mapZoom READ zoom CONSTANT)
  Q_PROPERTY(double mapBearing READ zero CONSTANT)
  Q_PROPERTY(double mapTilt READ zero CONSTANT)
  Q_PROPERTY(double vehicleOffsetY READ zero CONSTANT)
  Q_PROPERTY(QString styleJson READ style CONSTANT)
  Q_PROPERTY(QString routeGeoJson READ empty CONSTANT)
  Q_PROPERTY(QString planGeoJson READ empty CONSTANT)
  Q_PROPERTY(QString planStopsGeoJson READ empty CONSTANT)
  Q_PROPERTY(QString overviewTraveledGeoJson READ empty CONSTANT)
  Q_PROPERTY(QString overviewMarkersGeoJson READ empty CONSTANT)
  Q_PROPERTY(QVariantList mapThemeLayers READ layers CONSTANT)
public:
  bool yes() const { return true; }
  bool no() const { return false; }
  double latitude() const { return 52.52; }
  double longitude() const { return 13.4; }
  double zoom() const { return 16; }
  double zero() const { return 0; }
  QString empty() const {
    return "{\"type\":\"FeatureCollection\",\"features\":[]}";
  }
  QVariantList layers() const { return {}; }
  QString style() const {
    QJsonObject base{
        {"version", 8},
        {"sources", QJsonObject{}},
        {"layers",
         QJsonArray{QJsonObject{
             {"id", "background"},
             {"type", "background"},
             {"paint", QJsonObject{{"background-color", "#10222f"}}}}}}};
    MapStyleConfig config;
    config.view2D = true;
    auto result = composeMapStyle(
        QJsonDocument(base).toJson(QJsonDocument::Compact), config);
    return QString::fromUtf8(result.json);
  }
};
class RideGroupMapRenderTest : public QObject {
  Q_OBJECT
private slots:
  void styledPeerFrame() {
    InMemoryMdbRepository repo;
    repo.set("settings", "ride-group.enabled", "true");
    repo.set("vehicle", "state", "parked");
    QTemporaryDir root;
    RideGroupService sharing(
        &repo, nullptr, std::make_shared<RideGroupJournal>(root.path(), false));
    QTRY_VERIFY(!sharing.busy());
    sharing.enableSharing();
    QTRY_VERIFY(!sharing.busy());
    auto id = [](int value) {
      return QString("00000000-0000-4000-8000-%1")
          .arg(value, 12, 10, QChar('0'));
    };
    MapProvider map;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("mapService", &map);
    engine.rootContext()->setContextProperty("rideGroupService", &sharing);
    QQmlComponent component(&engine);
    QString directory = QFINDTESTDATA("../qml/widgets/map");
    component.setData(
        QString("import QtQuick\nimport \"%1\"\nWindow { visible:true; "
                "width:480; height:480; MapViewContentLegacy { "
                "objectName:\"live-map\"; anchors.fill:parent } }")
            .arg(QUrl::fromLocalFile(directory).toString())
            .toUtf8(),
        QUrl::fromLocalFile(directory + "/render-test.qml"));
    QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading,
                             10000);
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(object.get());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->requestActivate();
    QTRY_COMPARE(QGuiApplication::applicationState(), Qt::ApplicationActive);
    QObject *view = object->findChild<QObject *>("live-map");
    QVERIFY(view);
    QTRY_VERIFY_WITH_TIMEOUT(view->property("styledOutputReady").toBool(),
                             10000);
    QJsonValue challenge;
    auto subscription = repo.subscribe(
        "ride-group:activity", [&](const QString &, const QString &message) {
          auto value = QJsonDocument::fromJson(message.toUtf8()).object();
          if (value["type"] == "ride_group_activity" &&
              value["active"].toBool())
            challenge = value["challenge"];
        });
    sharing.setMapVisible(false);
    sharing.setMapVisible(true);
    QVERIFY(challenge.isString());
    repo.unsubscribe("ride-group:activity", subscription);
    double now = QDateTime::currentMSecsSinceEpoch();
    QJsonArray descriptors;
    for (int n = 0; n < 5; ++n)
      descriptors.append(QJsonObject{{"peer_id", id(30 + n)},
                                     {"label", QString("Rider %1").arg(n + 1)},
                                     {"palette_index", n}});
    QJsonObject grant{
        {"v", 2},
        {"type", "ride_group_control"},
        {"connection_nonce", id(1)},
        {"action", "grant"},
        {"session_id", id(2)},
        {"member_id", id(3)},
        {"revision", 1},
        {"transport_epoch", 1},
        {"stream_id", id(4)},
        {"challenge", challenge},
        {"server_time_ms", now},
        {"lease_ttl_ms", 10000},
        {"cadence_ms", 2000},
        {"peers", descriptors},
        {"consent_generation", 1},
        {"stop_operation_id", QJsonValue(QJsonValue::Null)},
        {"stop_result", QJsonValue(QJsonValue::Null)},
        {"stop_expected_generation", QJsonValue(QJsonValue::Null)}};
    sharing.receive(
        "ride-group:control",
        QString::fromUtf8(QJsonDocument(grant).toJson(QJsonDocument::Compact)));
    for (int n = 0; n < 5; ++n) {
      QJsonObject point{{"v", 2},
                        {"type", "ride_group_peer"},
                        {"session_id", id(2)},
                        {"revision", 1},
                        {"transport_epoch", 1},
                        {"stream_id", id(4)},
                        {"peer_stream_id", id(40 + n)},
                        {"peer_transport_epoch", 1},
                        {"seq", 1},
                        {"peer_id", id(30 + n)},
                        {"label", QString("Rider %1").arg(n + 1)},
                        {"palette_index", n},
                        {"captured_at_ms", now - 100},
                        {"timestamp_source", "producer_fix"},
                        {"lat", 52.52 + (n - 2) * 0.00025},
                        {"lon", 13.4 + (n - 2) * 0.0002},
                        {"accuracy_m", 3},
                        {"heading_deg", QJsonValue(QJsonValue::Null)},
                        {"speed_mps", QJsonValue(QJsonValue::Null)},
                        {"motion_source", "unknown"},
                        {"received_at_ms", now},
                        {"server_time_ms", now},
                        {"valid_for_ms", 1000}};
      sharing.receive("ride-group:peers",
                      QString::fromUtf8(
                          QJsonDocument(point).toJson(QJsonDocument::Compact)));
    }
    QCOMPARE(sharing.peerCount(), 5);
    QTest::qWait(250);
    QImage image = window->grabWindow();
    QCOMPARE(image.size(), QSize(480, 480));
    int markers = 0;
    for (int y = 0; y < image.height(); ++y)
      for (int x = 0; x < image.width(); ++x) {
        QColor c = image.pixelColor(x, y);
        if ((c.red() > 180 && c.green() > 70 && c.blue() < 100) ||
            (c.blue() > 140 && c.green() > 100 && c.red() < 80) ||
            (c.red() > 150 && c.blue() > 140 && c.green() < 100))
          ++markers;
      }
    QVERIFY2(markers > 60, "styled frame did not contain colored peer markers");
    const QByteArray output = qgetenv("RIDE_GROUP_RENDER_CAPTURE");
    if (!output.isEmpty())
      QVERIFY(image.save(QString::fromUtf8(output)));
  }
};
QTEST_MAIN(RideGroupMapRenderTest)
#include "RideGroupMapRenderTest.moc"
