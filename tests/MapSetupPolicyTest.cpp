#include <QtTest>

#include "controllers/MapSetupPolicy.h"

using namespace MapSetupPolicy;

namespace {

Inputs ready()
{
    Inputs in;
    in.mode = static_cast<int>(ScootEnums::SetupMode::Both);
    in.mapsOk = true;
    in.routingOk = true;
    in.downloadStatus = static_cast<int>(ScootEnums::MapDownloadStatus::Idle);
    in.online = true;
    in.hasGps = true;
    return in;
}

}

class MapSetupPolicyTest : public QObject
{
    Q_OBJECT

private slots:
    void allInstalledDownloadsNothing();
    void modeForcesSingleComponent();
    void bothModeFetchesWhatIsMissing();
    void updateRefreshesWhatTheModeAsksFor();
    void canDownloadNeedsIdleOnlineGps();
    void partialResumeFollowsMode();
    void buttonPrecedence();
    void titleSelection();
    void bodySelection();
};

void MapSetupPolicyTest::allInstalledDownloadsNothing()
{
    const Inputs in = ready();
    QVERIFY(!willDownloadAnything(in));
    QVERIFY(!canDownload(in));
    QCOMPARE(body(in), Body::AllSet);
}

void MapSetupPolicyTest::modeForcesSingleComponent()
{
    Inputs in = ready();
    in.mode = static_cast<int>(ScootEnums::SetupMode::Routing);
    in.mapsOk = false;
    in.routingOk = false;
    QVERIFY(!showDisplayRow(in));
    QVERIFY(showRoutingRow(in));
    QVERIFY(!willDownloadDisplay(in));
    QVERIFY(willDownloadRouting(in));
}

void MapSetupPolicyTest::bothModeFetchesWhatIsMissing()
{
    Inputs in = ready();
    in.routingOk = false;
    QVERIFY(!willDownloadDisplay(in));
    QVERIFY(willDownloadRouting(in));
    QCOMPARE(body(in), Body::RoutingOnly);

    in.mapsOk = false;
    QVERIFY(willDownloadDisplay(in));
    QCOMPARE(body(in), Body::Both);
}

void MapSetupPolicyTest::updateRefreshesWhatTheModeAsksFor()
{
    Inputs in = ready();
    in.updateAvailable = true;
    QVERIFY(willDownloadDisplay(in));
    QVERIFY(willDownloadRouting(in));

    in.mode = static_cast<int>(ScootEnums::SetupMode::DisplayMaps);
    QVERIFY(willDownloadDisplay(in));
    QVERIFY(!willDownloadRouting(in));
}

void MapSetupPolicyTest::canDownloadNeedsIdleOnlineGps()
{
    Inputs in = ready();
    in.mapsOk = false;
    QVERIFY(canDownload(in));

    in.online = false;
    QVERIFY(!canDownload(in));

    in.online = true;
    in.hasGps = false;
    QVERIFY(!canDownload(in));

    in.hasGps = true;
    in.downloadStatus = static_cast<int>(ScootEnums::MapDownloadStatus::Downloading);
    QVERIFY(!canDownload(in));
}

void MapSetupPolicyTest::partialResumeFollowsMode()
{
    Inputs in = ready();
    in.hasPartialRouting = true;
    QVERIFY(hasRelevantPartial(in));

    in.mode = static_cast<int>(ScootEnums::SetupMode::DisplayMaps);
    QVERIFY(!hasRelevantPartial(in));

    in.hasPartialDisplay = true;
    QVERIFY(hasRelevantPartial(in));
}

void MapSetupPolicyTest::buttonPrecedence()
{
    Inputs in = ready();
    QCOMPARE(buttonAction(in), ButtonAction::Download);

    in.hasPartialDisplay = true;
    QCOMPARE(buttonAction(in), ButtonAction::Resume);

    // An available update outranks a partial download.
    in.updateAvailable = true;
    QCOMPARE(buttonAction(in), ButtonAction::Update);
}

void MapSetupPolicyTest::titleSelection()
{
    Inputs in = ready();
    QCOMPARE(title(in), Title::Setup);

    // Forced modes name their component regardless of health.
    in.mode = static_cast<int>(ScootEnums::SetupMode::DisplayMaps);
    QCOMPARE(title(in), Title::MapsUnavailable);
    in.mode = static_cast<int>(ScootEnums::SetupMode::Routing);
    QCOMPARE(title(in), Title::RoutingUnavailable);

    in.mode = static_cast<int>(ScootEnums::SetupMode::Both);
    in.mapsOk = false;
    QCOMPARE(title(in), Title::MapsUnavailable);
    in.routingOk = false;
    QCOMPARE(title(in), Title::BothUnavailable);
    in.mapsOk = true;
    QCOMPARE(title(in), Title::RoutingUnavailable);
}

void MapSetupPolicyTest::bodySelection()
{
    Inputs in = ready();
    in.mapsOk = false;
    in.routingOk = true;
    QCOMPARE(body(in), Body::DisplayOnly);
}

QTEST_MAIN(MapSetupPolicyTest)
#include "MapSetupPolicyTest.moc"
