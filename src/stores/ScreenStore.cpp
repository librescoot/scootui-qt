#include "ScreenStore.h"
#include "SettingsStore.h"
#include "../repositories/MdbRepository.h"

#include <QMetaEnum>

ScreenStore::ScreenStore(SettingsStore *settings, MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
    applyMode(settings->mode());
    connect(settings, &SettingsStore::modeChanged, this, [this, settings]() {
        applyMode(settings->mode());
    });
}

bool ScreenStore::isBrakeNavigated(ScootEnums::ScreenMode mode)
{
    switch (mode) {
    case ScootEnums::ScreenMode::About:
    case ScootEnums::ScreenMode::AddressSelection:
    case ScootEnums::ScreenMode::NavigationSetup:
    case ScootEnums::ScreenMode::Faults:
    case ScootEnums::ScreenMode::SystemInfo:
    case ScootEnums::ScreenMode::UpdateModeInfo:
    case ScootEnums::ScreenMode::UpdateChannel:
    case ScootEnums::ScreenMode::HopOnInfo:
        return true;
    default:
        return false;
    }
}

void ScreenStore::publishMenuOpen()
{
    if (!m_repo) return;
    m_repo->set(QStringLiteral("dashboard"), QStringLiteral("menu-open"),
                isBrakeNavigated(m_currentScreen) ? QStringLiteral("true")
                                                 : QStringLiteral("false"));
}

QString ScreenStore::screenName(ScootEnums::ScreenMode mode)
{
    const QMetaEnum e = QMetaEnum::fromType<ScootEnums::ScreenMode>();
    return QString::fromLatin1(e.valueToKey(static_cast<int>(mode)));
}

bool ScreenStore::screenModeFromName(const QString &name, ScootEnums::ScreenMode &out)
{
    const QMetaEnum e = QMetaEnum::fromType<ScootEnums::ScreenMode>();
    bool ok = false;
    const int v = e.keyToValue(name.toLatin1().constData(), &ok);
    if (ok) out = static_cast<ScootEnums::ScreenMode>(v);
    return ok;
}

void ScreenStore::publishScreen()
{
    if (!m_repo) return;
    m_repo->set(QStringLiteral("dashboard"), QStringLiteral("remote-screen"),
                screenName(m_currentScreen));
}

void ScreenStore::applyScreenLocally(ScootEnums::ScreenMode mode)
{
    if (mode == m_currentScreen) return;
    m_currentScreen = mode;
    publishMenuOpen();
    emit currentScreenChanged();
}

SyncSettings ScreenStore::syncSettings() const
{
    return {
        QStringLiteral("dashboard"),
        500,
        {
            {QStringLiteral("remote-screen"), QStringLiteral("remote-screen")},
        },
        {},
        {}
    };
}

void ScreenStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable != QLatin1String("remote-screen") || value.isEmpty())
        return;
    ScootEnums::ScreenMode mode;
    if (screenModeFromName(value, mode))
        applyScreenLocally(mode);
}

void ScreenStore::applyMode(const QString &mode)
{
    ScootEnums::ScreenMode target = ScootEnums::ScreenMode::Cluster;
    if (mode == QLatin1String("navigation"))
        target = ScootEnums::ScreenMode::Map;
    else if (mode == QLatin1String("debug"))
        target = ScootEnums::ScreenMode::Debug;
    else if (mode == QLatin1String("motion-debug"))
        target = ScootEnums::ScreenMode::MotionDebug;

    setScreen(static_cast<int>(target));
}

void ScreenStore::setScreen(int screen)
{
    auto mode = static_cast<ScootEnums::ScreenMode>(screen);
    if (mode == m_currentScreen) return;
    applyScreenLocally(mode);
    publishScreen();
}

void ScreenStore::showAddressSelection()
{
    m_screenBeforeAddressSelection = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::AddressSelection));
}

// Cancelling out. Confirming a destination hands over to the map instead and
// does not come through here.
void ScreenStore::closeAddressSelection()
{
    setScreen(static_cast<int>(m_screenBeforeAddressSelection));
}

void ScreenStore::showAbout()
{
    m_screenBeforeAbout = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::About));
}

void ScreenStore::closeAbout()
{
    setScreen(static_cast<int>(m_screenBeforeAbout));
}

void ScreenStore::showNavigationSetup(int setupMode)
{
    m_screenBeforeNavSetup = m_currentScreen;
    if (setupMode != m_setupMode) {
        m_setupMode = setupMode;
        emit setupModeChanged();
    }
    setScreen(static_cast<int>(ScootEnums::ScreenMode::NavigationSetup));
}

void ScreenStore::closeNavigationSetup()
{
    setScreen(static_cast<int>(m_screenBeforeNavSetup));
}

void ScreenStore::showFaults()
{
    m_screenBeforeFaults = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::Faults));
}

void ScreenStore::closeFaults()
{
    setScreen(static_cast<int>(m_screenBeforeFaults));
}

void ScreenStore::showSystemInfo(int page)
{
    if (page != m_systemInfoPage) {
        m_systemInfoPage = page;
        emit systemInfoPageChanged();
    }
    m_screenBeforeSystemInfo = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::SystemInfo));
}

void ScreenStore::closeSystemInfo()
{
    setScreen(static_cast<int>(m_screenBeforeSystemInfo));
}

void ScreenStore::showUpdateModeInfo()
{
    m_screenBeforeUpdateModeInfo = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::UpdateModeInfo));
}

void ScreenStore::closeUpdateModeInfo()
{
    setScreen(static_cast<int>(m_screenBeforeUpdateModeInfo));
}

void ScreenStore::confirmUpdateMode()
{
    emit umsModeRequested();
    setScreen(static_cast<int>(m_screenBeforeUpdateModeInfo));
}

void ScreenStore::showUpdateChannel()
{
    m_screenBeforeUpdateChannel = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::UpdateChannel));
}

void ScreenStore::closeUpdateChannel()
{
    setScreen(static_cast<int>(m_screenBeforeUpdateChannel));
}

void ScreenStore::showHopOnInfo()
{
    m_screenBeforeHopOnInfo = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::HopOnInfo));
}

void ScreenStore::closeHopOnInfo()
{
    setScreen(static_cast<int>(m_screenBeforeHopOnInfo));
}

void ScreenStore::enterHopOnLock()
{
    m_screenBeforeHopOnLock = m_currentScreen;
    setScreen(static_cast<int>(ScootEnums::ScreenMode::Cluster));
}

void ScreenStore::exitHopOnLock()
{
    setScreen(static_cast<int>(m_screenBeforeHopOnLock));
}
