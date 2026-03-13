#include "UmsLogStore.h"

UmsLogStore::UmsLogStore(MdbRepository *repo, QObject *parent)
    : QObject(parent), m_repo(repo)
{
    m_timer.setInterval(500);
    connect(&m_timer, &QTimer::timeout, this, &UmsLogStore::poll);
}

void UmsLogStore::startPolling()
{
    if (!m_timer.isActive()) {
        poll();
        m_timer.start();
    }
}

void UmsLogStore::stopPolling()
{
    m_timer.stop();
}

void UmsLogStore::clear()
{
    if (!m_logEntries.isEmpty()) {
        m_logEntries.clear();
        emit logEntriesChanged();
    }
}

// Strip leading "YYYY-MM-DD HH:MM:SS " (20 chars) from Go log entries.
// Checks position 10 (space) and 16 (colon between MM and SS) to identify format.
QString UmsLogStore::stripTimestamp(const QString &entry)
{
    if (entry.length() > 20 && entry[10] == QLatin1Char(' ') && entry[16] == QLatin1Char(':'))
        return entry.mid(20);
    return entry;
}

void UmsLogStore::poll()
{
    QStringList raw = m_repo->lrange(QStringLiteral("usb:log"), 0, 19);
    QStringList entries;
    entries.reserve(raw.size());
    for (const QString &e : raw)
        entries.append(stripTimestamp(e));
    // Show last 4 (tail of list)
    if (entries.size() > 4)
        entries = entries.mid(entries.size() - 4);
    if (entries != m_logEntries) {
        m_logEntries = entries;
        emit logEntriesChanged();
    }
}
