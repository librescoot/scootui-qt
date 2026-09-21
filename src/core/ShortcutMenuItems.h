#pragma once

#include <QString>
#include <QStringList>

// Helpers for dashboard.shortcut-menu.items: the ordered JSON array that
// defines which actions the shortcut menu offers and in what order. Entries
// are fixed action ids or destination:<uuid>:<icon> tokens; a token's position
// among destination tokens is its quick-nav slot.
namespace ShortcutMenuItems {

inline constexpr char SettingsKey[] = "dashboard.shortcut-menu.items";

QStringList defaultItems();
QStringList parse(const QString &json, bool *ok = nullptr);
QString serialize(const QStringList &items);

bool isDestination(const QString &token, QString *uuid = nullptr, QString *icon = nullptr);
bool uuidEquals(const QString &left, const QString &right);
QString destinationToken(const QString &uuid, const QString &icon);

// Destination tokens in quick-nav slot order (1-based among destinations).
QStringList destinationUuids(const QStringList &items);
QString destinationTokenAt(const QStringList &items, int slot);

// Slot edits return the updated list. Assigning a slot that another token
// holds swaps the two, matching the menu's replacement semantics.
QStringList setDestinationSlot(const QStringList &items, const QString &uuid, int slot);
QStringList clearDestinationSlot(const QStringList &items, int slot);
QStringList setDestinationIcon(const QStringList &items, const QString &uuid, const QString &icon);
QStringList withoutDestination(const QStringList &items, const QString &uuid);

} // namespace ShortcutMenuItems
