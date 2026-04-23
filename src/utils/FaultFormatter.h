#pragma once

#include <QString>
#include <QList>

class Translations;

enum class FaultSeverity { Critical, Warning };

class FaultFormatter
{
public:
    static QString formatSingleFault(int code, Translations *tr);
    static QString formatMultipleFaults(const QList<int> &codes, Translations *tr);
    static bool hasAnyCritical(const QList<int> &codes);
    static QString getMultipleFaultsTitle(const QList<int> &codes, Translations *tr);
    static FaultSeverity getSeverity(int code);
    static QString getDescription(int code, Translations *tr);

    // ECU faults (Exx). Prefix "E", different code→description mapping.
    static QString formatEcuFault(int code, Translations *tr);
    static FaultSeverity getEcuSeverity(int code);
    static QString getEcuDescription(int code, Translations *tr);

    // Source-aware helpers used by FaultsStore to render cross-service entries.
    // `source` is the `group` value from events:faults ("battery:0", "engine-ecu",
    // "ble", "internet", "vehicle", "keycard", "pm", "ota").
    static QString sourceLabel(const QString &source);
    static QString codeLabel(const QString &source, int code);
    static QString describeFault(const QString &source, int code, Translations *tr);
    static FaultSeverity faultSeverity(const QString &source, int code);
};
