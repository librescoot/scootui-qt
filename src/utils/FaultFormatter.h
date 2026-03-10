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
};
