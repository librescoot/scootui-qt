#include "repositories/HiredisWorker.h"
#include <QtTest>
#include <atomic>
#include <hiredis/hiredis.h>
#include <mutex>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

class RedisEndpoint {
public:
  RedisEndpoint() {
    listener = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(listener, reinterpret_cast<sockaddr *>(&address),
             sizeof(address)) != 0 ||
        listen(listener, 1) != 0)
      qFatal("local Redis endpoint unavailable");
    socklen_t length = sizeof(address);
    getsockname(listener, reinterpret_cast<sockaddr *>(&address), &length);
    port = ntohs(address.sin_port);
    thread = std::thread([this] { serve(); });
  }
  ~RedisEndpoint() {
    stopped = true;
    int fd = client.load();
    if (fd >= 0)
      shutdown(fd, SHUT_RDWR);
    shutdown(listener, SHUT_RDWR);
    close(listener);
    thread.join();
  }
  int count() {
    std::lock_guard<std::mutex> guard(mutex);
    return published.size();
  }
  quint16 port = 0;

private:
  void serve() {
    int fd = accept(listener, nullptr, nullptr);
    client = fd;
    if (fd < 0)
      return;
    redisReader *reader = redisReaderCreate();
    char buffer[4096];
    while (!stopped) {
      ssize_t count = recv(fd, buffer, sizeof(buffer), 0);
      if (count <= 0)
        break;
      redisReaderFeed(reader, buffer, count);
      void *raw = nullptr;
      while (redisReaderGetReply(reader, &raw) == REDIS_OK && raw) {
        auto *reply = static_cast<redisReply *>(raw);
        bool publish =
            reply->type == REDIS_REPLY_ARRAY && reply->elements >= 3 &&
            QByteArray(reply->element[0]->str, reply->element[0]->len)
                    .toUpper() == "PUBLISH";
        if (publish) {
          std::lock_guard<std::mutex> guard(mutex);
          published.append(
              QByteArray(reply->element[2]->str, reply->element[2]->len));
        }
        const QByteArray response =
            publish ? QByteArray(":1\r\n") : QByteArray("*0\r\n");
        send(fd, response.constData(), response.size(), MSG_NOSIGNAL);
        freeReplyObject(reply);
        raw = nullptr;
      }
    }
    redisReaderFree(reader);
    close(fd);
    client = -1;
  }
  int listener = -1;
  std::atomic<int> client{-1};
  std::atomic<bool> stopped{false};
  std::thread thread;
  std::mutex mutex;
  QList<QByteArray> published;
};
class RideGroupIpcWriterTest : public QObject {
  Q_OBJECT
private slots:
  void actualRedisAdmissionChecksExpiryGenerationAndPermit() {
    RedisEndpoint endpoint;
    HiredisWorker worker("127.0.0.1", endpoint.port);
    worker.registerChannel("test", 99999);
    worker.start();
    quint64 generation = worker.connectionGeneration();
    QVERIFY(generation > 0);
    worker.doPublishTransient("ride-group:activity", "accepted",
                              QDeadlineTimer(1000), generation,
                              [] { return true; });
    QCOMPARE(endpoint.count(), 1);
    worker.doPublishTransient("ride-group:activity", "denied",
                              QDeadlineTimer(1000), generation,
                              [] { return false; });
    QCOMPARE(endpoint.count(), 1);
    worker.doPublishTransient("ride-group:activity", "expired",
                              QDeadlineTimer(0), generation,
                              [] { return true; });
    QCOMPARE(endpoint.count(), 1);
    worker.doPublishTransient("ride-group:activity", "retired",
                              QDeadlineTimer(1000), generation - 1,
                              [] { return true; });
    QCOMPARE(endpoint.count(), 1);
    worker.stop();
    worker.doPublishTransient("ride-group:activity", "disconnected",
                              QDeadlineTimer(1000), generation,
                              [] { return true; });
    QCOMPARE(endpoint.count(), 1);
  }
};
QTEST_GUILESS_MAIN(RideGroupIpcWriterTest)
#include "RideGroupIpcWriterTest.moc"
