#pragma once

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QVariantList>
#include <array>

class DoctorService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList boards READ boards NOTIFY changed)
    Q_PROPERTY(bool running READ running NOTIFY changed)
public:
    struct Command { QString program; QStringList arguments; };
    explicit DoctorService(QObject *parent = nullptr);
    DoctorService(Command dbc, Command mdb, int timeoutMs, QObject *parent = nullptr);
    ~DoctorService() override;

    QVariantList boards() const;
    bool running() const;
    Q_INVOKABLE void start();
    Q_INVOKABLE void cancel();

    static bool decodeReport(const QByteArray &data, QVariantList &checks, QString &error);
signals:
    void changed();
private:
    struct Board {
        QString id;
        QString state = QStringLiteral("idle");
        QString error;
        QString completedAt;
        QVariantList checks;
        QByteArray output;
        qint64 receivedBytes = 0;
        QPointer<QProcess> process;
    };
    void launch(int index, quint64 generation);
    void finish(int index, quint64 generation, const QString &error = {});
    std::array<Command, 2> m_commands;
    std::array<Board, 2> m_boards;
    int m_timeoutMs;
    quint64 m_generation = 0;
};
