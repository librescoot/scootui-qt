#include "ModemStore.h"

ModemStore::ModemStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
}

SyncSettings ModemStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("modem"), 5000,
        {
            {QStringLiteral("powerState"), QStringLiteral("power-state")},
            {QStringLiteral("simState"), QStringLiteral("sim-state")},
            {QStringLiteral("simLock"), QStringLiteral("sim-lock")},
            {QStringLiteral("operatorName"), QStringLiteral("operator-name")},
            {QStringLiteral("operatorCode"), QStringLiteral("operator-code")},
            {QStringLiteral("registration"), QStringLiteral("registration")},
            {QStringLiteral("registrationFail"), QStringLiteral("registration-fail")},
            {QStringLiteral("isRoaming"), QStringLiteral("is-roaming")},
            {QStringLiteral("errorState"), QStringLiteral("error-state")},
            {QStringLiteral("pinAction"), QStringLiteral("pin-action")},
            {QStringLiteral("apnAction"), QStringLiteral("apn-action")},
        },
        {},
        {}
    };
}

void ModemStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("power-state")) {
        if (value != m_powerState) { m_powerState = value; emit powerStateChanged(); }
    } else if (variable == QLatin1String("sim-state")) {
        if (value != m_simState) { m_simState = value; emit simStateChanged(); }
    } else if (variable == QLatin1String("sim-lock")) {
        if (value != m_simLock) { m_simLock = value; emit simLockChanged(); }
    } else if (variable == QLatin1String("operator-name")) {
        if (value != m_operatorName) { m_operatorName = value; emit operatorNameChanged(); }
    } else if (variable == QLatin1String("operator-code")) {
        if (value != m_operatorCode) { m_operatorCode = value; emit operatorCodeChanged(); }
    } else if (variable == QLatin1String("registration")) {
        if (value != m_registration) { m_registration = value; emit registrationChanged(); }
    } else if (variable == QLatin1String("registration-fail")) {
        if (value != m_registrationFail) { m_registrationFail = value; emit registrationFailChanged(); }
    } else if (variable == QLatin1String("is-roaming")) {
        bool v = (value == QLatin1String("true"));
        if (v != m_isRoaming) { m_isRoaming = v; emit isRoamingChanged(); }
    } else if (variable == QLatin1String("error-state")) {
        if (value != m_errorState) { m_errorState = value; emit errorStateChanged(); }
    } else if (variable == QLatin1String("pin-action")) {
        if (value != m_pinAction) { m_pinAction = value; emit pinActionChanged(); }
    } else if (variable == QLatin1String("apn-action")) {
        if (value != m_apnAction) { m_apnAction = value; emit apnActionChanged(); }
    }
}
