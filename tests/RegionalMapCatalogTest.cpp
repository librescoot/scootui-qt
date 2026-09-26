#include <QtTest>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QFile>

#include "services/RegionalMapCatalog.h"

class RegionalMapCatalogTest : public QObject
{
    Q_OBJECT
private slots:
    void scanAndSelect();
};

void RegionalMapCatalogTest::scanAndSelect()
{
    QTemporaryDir maps, routing;
    QVERIFY(maps.isValid());
    QVERIFY(routing.isValid());
    const auto add = [&](const QString &slug, const QString &bounds) {
        const QString path = maps.filePath(QStringLiteral("tiles_") + slug + QStringLiteral(".mbtiles"));
        const QString connection = QStringLiteral("test_") + slug;
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery query(db);
            QVERIFY(query.exec(QStringLiteral("CREATE TABLE metadata (name TEXT, value TEXT)")));
            query.prepare(QStringLiteral("INSERT INTO metadata VALUES ('bounds', ?)"));
            query.addBindValue(bounds);
            QVERIFY(query.exec());
            db.close();
        }
        QSqlDatabase::removeDatabase(connection);
        QFile archive(routing.filePath(QStringLiteral("valhalla_tiles_") + slug + QStringLiteral(".tar")));
        QVERIFY(archive.open(QIODevice::WriteOnly));
    };
    add(QStringLiteral("west"), QStringLiteral("0,0,10,10"));
    add(QStringLiteral("east"), QStringLiteral("9,0,20,10"));
    add(QStringLiteral("invalid"), QStringLiteral("nonsense"));
    const auto packs = RegionalMapCatalog::scan(maps.path(), routing.path());
    QCOMPARE(packs.size(), 2);
    const auto *west = RegionalMapCatalog::select(packs, 5, 5);
    QVERIFY(west);
    QCOMPARE(west->slug, QStringLiteral("west"));
    QCOMPARE(RegionalMapCatalog::select(packs, 5, 9.5, west->slug)->slug, west->slug);
    QCOMPARE(RegionalMapCatalog::select(packs, 5, 15, west->slug)->slug, QStringLiteral("east"));
    QVERIFY(!RegionalMapCatalog::select(packs, 50, 50));
}

QTEST_MAIN(RegionalMapCatalogTest)
#include "RegionalMapCatalogTest.moc"
