#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>

#include "repositories/BootPrefetch.h"

// Answers every HGETALL with one field, "k" = the channel name.
class FakeRedis : public QObject
{
    Q_OBJECT
public:
    quint16 port = 0;

    bool listen()
    {
        m_server = new QTcpServer(this);
        if (!m_server->listen(QHostAddress::LocalHost, 0))
            return false;
        port = m_server->serverPort();
        connect(m_server, &QTcpServer::newConnection, this, [this]() {
            QTcpSocket *socket = m_server->nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [socket]() {
                const QByteArray request = socket->readAll();
                const int lastBulk = request.lastIndexOf("\r\n$");
                const int nameStart = request.indexOf("\r\n", lastBulk + 3) + 2;
                const QByteArray channel = request.mid(nameStart).trimmed();
                socket->write("*2\r\n$1\r\nk\r\n$" + QByteArray::number(channel.size())
                              + "\r\n" + channel + "\r\n");
            });
        });
        return true;
    }

private:
    QTcpServer *m_server = nullptr;
};

class BootPrefetchTest : public QObject
{
    Q_OBJECT

private slots:
    void fetchesEveryChannel();
    void givesUpWithoutServer();
};

void BootPrefetchTest::fetchesEveryChannel()
{
    FakeRedis fake;
    QVERIFY(fake.listen());

    BootPrefetch prefetch(QStringLiteral("127.0.0.1"), fake.port, QString(),
                          {QStringLiteral("vehicle"), QStringLiteral("settings")});
    prefetch.start();
    QTRY_VERIFY_WITH_TIMEOUT(prefetch.waitFinished(0), 5000);

    QVERIFY(prefetch.succeeded());
    QVERIFY(!prefetch.usedBackup());
    QVERIFY(prefetch.finishedAtMs() >= 0);
    const auto result = prefetch.take();
    QCOMPARE(result.size(), 2);
    QCOMPARE(result.value(QStringLiteral("vehicle")).value(QStringLiteral("k")),
             QStringLiteral("vehicle"));
    QCOMPARE(result.value(QStringLiteral("settings")).value(QStringLiteral("k")),
             QStringLiteral("settings"));
}

void BootPrefetchTest::givesUpWithoutServer()
{
    BootPrefetch prefetch(QStringLiteral("127.0.0.1"), 1, QString(),
                          {QStringLiteral("vehicle")}, 600);
    prefetch.start();
    QVERIFY(prefetch.waitFinished(5000));
    QVERIFY(!prefetch.succeeded());
    QVERIFY(prefetch.take().isEmpty());
}

QTEST_MAIN(BootPrefetchTest)
#include "BootPrefetchTest.moc"
