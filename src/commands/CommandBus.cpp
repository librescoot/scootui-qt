#include "CommandBus.h"

#include <QDebug>

#include "../models/Enums.h"
#include "../repositories/MdbRepository.h"
#include "../repositories/RedisSchema.h"

CommandBus::CommandBus(MdbRepository *repo, QObject *parent)
    : QObject(parent)
    , m_repo(repo)
{
}

void CommandBus::lockVehicle()
{
    m_repo->push(RedisSchema::list::ScooterState, QStringLiteral("lock"));
}

void CommandBus::toggleHazards(int currentBlinkerState)
{
    const bool isBoth =
        currentBlinkerState == static_cast<int>(ScootEnums::BlinkerState::Both);
    const QString cmd = isBoth ? QStringLiteral("off") : QStringLiteral("both");
    qDebug() << "Toggle hazards: blinkerState=" << currentBlinkerState
             << "pushing=" << cmd;
    m_repo->push(RedisSchema::list::ScooterBlinker, cmd);
}

void CommandBus::hopOnEngage()
{
    m_repo->push(RedisSchema::list::ScooterHopOn, QStringLiteral("engage"));
}

void CommandBus::hopOnEngageLearning()
{
    m_repo->push(RedisSchema::list::ScooterHopOn, QStringLiteral("engage-learning"));
}

void CommandBus::hopOnRelease()
{
    m_repo->push(RedisSchema::list::ScooterHopOn, QStringLiteral("release"));
}

void CommandBus::holdDbc(const QString &reason)
{
    m_repo->push(RedisSchema::list::ScooterDbcHold, reason);
}

void CommandBus::releaseDbcHold()
{
    m_repo->push(RedisSchema::list::ScooterDbcHold, QStringLiteral("release"));
}

void CommandBus::deleteAllBluetoothBonds()
{
    m_repo->push(RedisSchema::list::ScooterBluetooth, QStringLiteral("delete-all-bonds"));
}

void CommandBus::applyServiceOverlay()
{
    m_repo->push(RedisSchema::list::SettingsOverlay, QStringLiteral("apply:service"));
}

void CommandBus::clearServiceOverlay()
{
    m_repo->push(RedisSchema::list::SettingsOverlay, QStringLiteral("clear:service"));
}

void CommandBus::setMenuOpen(bool open)
{
    m_repo->set(RedisSchema::hash::Dashboard, QStringLiteral("menu-open"),
                open ? QStringLiteral("true") : QStringLiteral("false"));
}

void CommandBus::setDebugMode(const QString &mode)
{
    m_repo->set(RedisSchema::hash::Dashboard, QStringLiteral("debug"), mode);
}
