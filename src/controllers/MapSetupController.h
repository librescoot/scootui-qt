#pragma once

#include <QObject>

class Navigator;
class NavigationAvailabilityService;
class InternetStore;
class GpsStore;
class MapDownloadService;

// Applies MapSetupPolicy to the live stores for the Navigation Setup screen:
// what the download button does, when it is enabled, which title/body to
// show, plus the auto region resolve once GPS and connectivity line up and
// the availability recheck after a completed download. The screen renders
// these properties and calls triggerDownload().
class MapSetupController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool showDisplayRow READ showDisplayRow NOTIFY policyChanged)
    Q_PROPERTY(bool showRoutingRow READ showRoutingRow NOTIFY policyChanged)
    Q_PROPERTY(bool willDownloadDisplay READ willDownloadDisplay NOTIFY policyChanged)
    Q_PROPERTY(bool willDownloadRouting READ willDownloadRouting NOTIFY policyChanged)
    Q_PROPERTY(bool willDownloadAnything READ willDownloadAnything NOTIFY policyChanged)
    Q_PROPERTY(bool canDownload READ canDownload NOTIFY policyChanged)
    Q_PROPERTY(int buttonAction READ buttonAction NOTIFY policyChanged)
    Q_PROPERTY(int title READ title NOTIFY policyChanged)
    Q_PROPERTY(int body READ body NOTIFY policyChanged)
    // Bytes the button would actually fetch, for the size label.
    Q_PROPERTY(double estimatedDownloadBytes READ estimatedDownloadBytes NOTIFY policyChanged)

public:
    // Mirrors of the MapSetupPolicy enums for QML.
    enum ButtonAction { Download = 0, Update, Resume };
    Q_ENUM(ButtonAction)
    enum Title { Setup = 0, MapsUnavailable, RoutingUnavailable, BothUnavailable };
    Q_ENUM(Title)
    enum Body { AllSet = 0, Both, DisplayOnly, RoutingOnly };
    Q_ENUM(Body)

    explicit MapSetupController(Navigator *navigator,
                                NavigationAvailabilityService *availability,
                                InternetStore *internet, GpsStore *gps,
                                MapDownloadService *download,
                                QObject *parent = nullptr);

    bool showDisplayRow() const;
    bool showRoutingRow() const;
    bool willDownloadDisplay() const;
    bool willDownloadRouting() const;
    bool willDownloadAnything() const;
    bool canDownload() const;
    int buttonAction() const;
    int title() const;
    int body() const;
    double estimatedDownloadBytes() const;

    Q_INVOKABLE void triggerDownload();

signals:
    void policyChanged();

private:
    void maybeResolveRegion();

    Navigator *m_navigator;
    NavigationAvailabilityService *m_availability;
    InternetStore *m_internet;
    GpsStore *m_gps;
    MapDownloadService *m_download;
};
