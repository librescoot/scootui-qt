#pragma once

#include <QObject>

class OtaStore;
class ToastService;
class Translations;

// Rider-facing OTA toasts, ported from Flutter's OtaCubit: announce each OTA
// state transition once (downloading / installing / pending-reboot / error).
// Progress ticks do not change the status, so they never re-announce.
//
// Only the component actually updating speaks, matching the status-bar OTA
// indicator: the DBC while it is busy, otherwise the MDB. An update already in
// flight when the dashboard starts up is announced too, which is the point -
// otherwise a scooter that boots busy looks stuck.
class OtaMonitor : public QObject
{
    Q_OBJECT

public:
    explicit OtaMonitor(OtaStore *ota, ToastService *toast, Translations *translations,
                        QObject *parent = nullptr);

private slots:
    void evaluate();

private:
    enum class Component { Dbc, Mdb };

    Component activeComponent() const;
    QString statusFor(Component component) const;
    QString versionFor(Component component) const;
    QString errorMessageFor(Component component) const;

    OtaStore *m_ota;
    ToastService *m_toast;
    Translations *m_translations;
    // component + status + error of the last announced state.
    QString m_lastKey;
};
