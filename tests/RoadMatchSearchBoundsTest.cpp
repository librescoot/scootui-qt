#include <QtTest>
#include "services/RoadMatchSearchBounds.h"

class RoadMatchSearchBoundsTest : public QObject {
    Q_OBJECT
private slots:
    void keepsCrossingAndBoundarySegments()
    {
        const RoadMatchSearchBounds b{0, 0, 10, 10};
        QVERIFY(b.intersects({{{-100, 5}, {100, 5}}}));
        QVERIFY(b.intersects({{{5, -100}, {5, 100}}}));
        QVERIFY(b.intersects({{{10, 10}, {20, 20}}}));
        QVERIFY(!b.intersects({{{20, 20}, {30, 30}}}));
        QVERIFY(!b.intersects({{{-10, 5}, {-5, 5}}, {{15, 5}, {20, 5}}}));
        QVERIFY(!b.intersects({}));
    }

    void retainsEntireAcceptanceRadius()
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr double earth = 6371000.0;
        constexpr int zoom = 14;
        constexpr double extent = 4096;
        constexpr double n = 1 << zoom;
        for (double lat : {-80.0, -52.0, 0.0, 52.0, 80.0}) {
            const double lon = 13.0;
            const int tx = int((lon + 180.0) / 360.0 * n);
            const int sy = int((1.0 - std::asinh(std::tan(lat * pi / 180.0)) / pi) * .5 * n);
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    const int x = tx + dx, y = int(n) - 1 - sy + dy;
                    const auto b = RoadMatchSearchBounds::around(lat, lon, x, y, zoom, extent);
                    for (int angle = 0; angle < 360; angle += 15) {
                        const double r = angle * pi / 180.0;
                        const double latitude = lat + 52.0 * std::sin(r) / earth * 180.0 / pi;
                        const double longitude = lon + 52.0 * std::cos(r) / earth * 180.0 / pi / std::cos(lat * pi / 180.0);
                        const QPointF p(((longitude + 180.0) / 360.0 * n - x) * extent,
                            ((1.0 - std::asinh(std::tan(latitude * pi / 180.0)) / pi) * .5 * n - (n - 1 - y)) * extent);
                        QVERIFY(b.intersects({{p, p + QPointF(.001, .001)}}));
                    }
                }
            }
        }
    }
};
QTEST_GUILESS_MAIN(RoadMatchSearchBoundsTest)
#include "RoadMatchSearchBoundsTest.moc"
