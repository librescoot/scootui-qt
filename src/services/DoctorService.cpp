#include "DoctorService.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QSet>
#include <QTimer>
#include <algorithm>
#include <utility>
#include <pwd.h>
#include <unistd.h>

namespace {
constexpr qint64 MaxReportBytes = 256 * 1024;
int rank(const QString &verdict)
{
    if (verdict == QLatin1String("attention")) return 3;
    if (verdict == QLatin1String("warning")) return 2;
    if (verdict == QLatin1String("unknown")) return 1;
    return 0;
}
}

DoctorService::DoctorService(QObject *parent)
    : DoctorService({QStringLiteral("/usr/bin/lsc"), {QStringLiteral("doctor"), QStringLiteral("--json")}},
                    {QStringLiteral("/usr/bin/ssh"), {QStringLiteral("-o"), QStringLiteral("BatchMode=yes"),
                     QStringLiteral("-o"), QStringLiteral("ConnectTimeout=5"),
                     QStringLiteral("-o"), QStringLiteral("ConnectionAttempts=1"),
                     QStringLiteral("-o"), QStringLiteral("ServerAliveInterval=5"),
                     QStringLiteral("-o"), QStringLiteral("ServerAliveCountMax=1"),
                     QStringLiteral("mdb"), QStringLiteral("lsc"), QStringLiteral("doctor"), QStringLiteral("--json")}},
                    40000, parent)
{
}

DoctorService::DoctorService(Command dbc, Command mdb, int timeoutMs, QObject *parent)
    : QObject(parent), m_commands{std::move(dbc), std::move(mdb)}, m_timeoutMs(timeoutMs)
{
    m_boards[0].id = QStringLiteral("DBC");
    m_boards[1].id = QStringLiteral("MDB");
}

DoctorService::~DoctorService()
{
    cancel();
    for (auto *process : findChildren<QProcess *>()) {
        if (process->state() != QProcess::NotRunning) {
            process->kill();
            process->waitForFinished(1000);
        }
    }
}

QVariantList DoctorService::boards() const
{
    QVariantList result;
    for (const auto &board : m_boards) {
        int attention = 0, warnings = 0, unknown = 0;
        for (const auto &entry : board.checks) {
            const auto verdict = entry.toMap().value(QStringLiteral("verdict")).toString();
            attention += verdict == QLatin1String("attention");
            warnings += verdict == QLatin1String("warning");
            unknown += verdict == QLatin1String("unknown");
        }
        result.append(QVariantMap{{"id", board.id}, {"state", board.state}, {"error", board.error},
            {"completedAt", board.completedAt}, {"checks", board.checks},
            {"attention", attention}, {"warnings", warnings}, {"unknown", unknown}});
    }
    return result;
}

bool DoctorService::running() const
{
    return std::any_of(m_boards.cbegin(), m_boards.cend(), [](const Board &board) {
        return board.state == QLatin1String("running");
    });
}

void DoctorService::start()
{
    if (running()) return;
    cancel();
    const auto generation = m_generation;
    for (auto &board : m_boards) {
        const auto id = board.id;
        board = Board{};
        board.id = id;
        board.state = QStringLiteral("running");
    }
    emit changed();
    for (int i = 0; i < 2; ++i) launch(i, generation);
}

void DoctorService::cancel()
{
    ++m_generation;
    bool changedState = false;
    for (auto &board : m_boards) {
        if (board.process) {
            board.process->disconnect(this);
            board.process->kill();
            board.process = nullptr;
        }
        if (board.state == QLatin1String("running")) {
            board.state = QStringLiteral("error");
            board.error = QStringLiteral("cancelled");
            changedState = true;
        }
    }
    if (changedState) emit changed();
}

void DoctorService::launch(int index, quint64 generation)
{
    auto *process = new QProcess(this);
    if (index == 1) {
        // ScootUI keeps its data in HOME=/data; SSH credentials belong to the account home.
        auto environment = QProcessEnvironment::systemEnvironment();
        if (const auto *account = getpwuid(geteuid())) {
            environment.insert(QStringLiteral("HOME"), QString::fromLocal8Bit(account->pw_dir));
        }
        process->setProcessEnvironment(environment);
    }
    m_boards[index].process = process;
    auto *deadline = new QTimer(process);
    deadline->setSingleShot(true);
    connect(deadline, &QTimer::timeout, this, [this, index, generation] {
        finish(index, generation, QStringLiteral("timeout"));
    });
    const auto drain = [this, index, generation, process] {
        if (generation != m_generation || m_boards[index].state != QLatin1String("running")) return;
        auto &board = m_boards[index];
        const auto out = process->readAllStandardOutput();
        const auto err = process->readAllStandardError();
        board.receivedBytes += out.size() + err.size();
        if (board.receivedBytes > MaxReportBytes) {
            finish(index, generation, QStringLiteral("too-large"));
            return;
        }
        board.output.append(out);
    };
    connect(process, &QProcess::readyReadStandardOutput, this, drain);
    connect(process, &QProcess::readyReadStandardError, this, drain);
    connect(process, &QProcess::finished, this, [this, index, generation, drain](int exitCode, QProcess::ExitStatus status) {
        drain();
        // Doctor exits 1 for a valid report containing faults. SSH uses 255 for
        // transport failures; a crash must never validate a partial report.
        finish(index, generation, status != QProcess::NormalExit || (exitCode != 0 && exitCode != 1)
            ? QStringLiteral("unavailable") : QString());
    });
    connect(process, &QProcess::finished, process, &QObject::deleteLater);
    connect(process, &QProcess::errorOccurred, this, [this, index, generation, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            finish(index, generation, QStringLiteral("unavailable"));
            process->deleteLater();
        }
    });
    process->start(m_commands[index].program, m_commands[index].arguments);
    deadline->start(m_timeoutMs);
}

void DoctorService::finish(int index, quint64 generation, const QString &failure)
{
    auto &board = m_boards[index];
    if (generation != m_generation || board.state != QLatin1String("running")) return;
    board.error = failure;
    if (board.error.isEmpty()) decodeReport(board.output, board.checks, board.error);
    board.state = board.error.isEmpty() ? QStringLiteral("done") : QStringLiteral("error");
    board.completedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    board.output.clear();
    if (board.process) {
        if (board.process->state() != QProcess::NotRunning) board.process->kill();
        board.process = nullptr;
    }
    emit changed();
}

bool DoctorService::decodeReport(const QByteArray &data, QVariantList &checks, QString &error)
{
    checks.clear();
    error = QStringLiteral("invalid");
    if (data.size() > MaxReportBytes) { error = QStringLiteral("too-large"); return false; }
    const auto document = QJsonDocument::fromJson(data);
    if (!document.isObject()) return false;
    const auto object = document.object();
    if (object.value("schema_version").toInt() != 1) { error = QStringLiteral("unsupported"); return false; }
    const auto entries = object.value("checks").toArray();
    QSet<QString> expected{"link", "kernel", "services", "ota", "faults", "storage"};
    if (entries.size() != expected.size()) return false;
    QVariantList parsed;
    for (const auto &entry : entries) {
        if (!entry.isObject()) return false;
        const auto check = entry.toObject();
        const auto name = check.value("name").toString();
        const auto verdict = check.value("verdict").toString();
        if (!expected.remove(name) || (verdict != "ok" && verdict != "warning" && verdict != "attention" && verdict != "unknown")) return false;
        QVariantMap row{{"name", name}, {"verdict", verdict}};
        for (const auto *field : {"detail", "hint"}) {
            const auto value = check.value(QLatin1String(field));
            if (!value.isUndefined() && (!value.isString() || value.toString().size() > 16384)) return false;
            row[QLatin1String(field)] = value.toString();
        }
        parsed.append(row);
    }
    std::stable_sort(parsed.begin(), parsed.end(), [](const QVariant &a, const QVariant &b) {
        return rank(a.toMap().value("verdict").toString()) > rank(b.toMap().value("verdict").toString());
    });
    checks = parsed;
    error.clear();
    return true;
}
