#include "services/RideGroupJournal.h"
#include "services/RideGroupJson.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
#include <sys/stat.h>
#include <unistd.h>

namespace {
QJsonObject record(QString phase, int revision = 1) {
  return {{"format", 1},
          {"revision", revision},
          {"blocked", phase != "idle"},
          {"operation_id",
           phase == "idle"
               ? QJsonValue(QJsonValue::Null)
               : QJsonValue("00000000-0000-4000-8000-000000000001")},
          {"phase", phase},
          {"expected_generation", QJsonValue(QJsonValue::Null)},
          {"committed_generation", QJsonValue(QJsonValue::Null)}};
}
} // namespace
class FaultJournal : public RideGroupJournal {
public:
  explicit FaultJournal(const QString &root) : RideGroupJournal(root, false) {}
  int faultAt = 0, calls = 0;

protected:
  bool checkpoint(WriteStage) override { return ++calls != faultAt; }
};
class RideGroupJournalTest : public QObject {
  Q_OBJECT
private slots:
  void missingRequiresExplicitCreation() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    RideGroupJournal journal(root.path(), false);
    QVERIFY(!journal.save(record("idle")));
    auto loaded = journal.load();
    QVERIFY(loaded.ok && loaded.missing);
    QVERIFY(journal.save(record("idle")));
    QVERIFY(!journal.save(record("idle")));
    auto result = journal.load();
    QVERIFY(result.ok);
    QCOMPARE(result.record["phase"].toString(), QString("idle"));
  }
  void recoveredPreparingIsDurablyUncertain() {
    QTemporaryDir root;
    {
      RideGroupJournal journal(root.path(), false);
      QVERIFY(journal.load().ok);
      QVERIFY(journal.save(record("preparing")));
    }
    {
      RideGroupJournal journal(root.path(), false);
      auto loaded = journal.load();
      QVERIFY(loaded.ok);
      QCOMPARE(loaded.record["phase"].toString(), QString("uncertain"));
      QCOMPARE(loaded.record["revision"].toInt(), 2);
      QVERIFY(loaded.record["expected_generation"].isNull());
    }
    RideGroupJournal again(root.path(), false);
    auto loaded = again.load();
    QVERIFY(loaded.ok);
    QCOMPARE(loaded.record["revision"].toInt(), 2);
    QCOMPARE(loaded.record["phase"].toString(), QString("uncertain"));
  }
  void exclusiveOwner() {
    QTemporaryDir root;
    RideGroupJournal first(root.path(), false), second(root.path(), false);
    QVERIFY(first.load().ok);
    QVERIFY(!second.load().ok);
  }
  void immutableFenceAndCounter() {
    QTemporaryDir root;
    RideGroupJournal journal(root.path(), false);
    QVERIFY(journal.load().ok);
    QVERIFY(journal.save(record("preparing")));
    auto prepared = record("prepared", 2);
    prepared["expected_generation"] = 7;
    QVERIFY(journal.save(prepared));
    auto changed = prepared;
    changed["revision"] = 3;
    changed["expected_generation"] = 8;
    QVERIFY(!journal.save(changed));
    changed = prepared;
    changed["revision"] = 3;
    changed["phase"] = "confirmed";
    changed["committed_generation"] = 8;
    QVERIFY(journal.save(changed));
    QVERIFY(!journal.save(prepared));
  }
  void symlinkAndUnsafePermissionsFailClosed() {
    QTemporaryDir root;
    RideGroupJournal journal(root.path(), false);
    QVERIFY(journal.load().ok);
    QString dir = root.path() + "/scootui/ride-group";
    QByteArray target = QFile::encodeName(dir + "/stop.json");
    QVERIFY(symlink("/dev/null", target.constData()) == 0);
    QVERIFY(!journal.load().ok);
    QVERIFY(unlink(target.constData()) == 0);
    QVERIFY(chmod(QFile::encodeName(dir).constData(), 0755) == 0);
    QVERIFY(!journal.save(record("idle")));
  }
  void storageDirectoryReplacementFailsClosed() {
    QTemporaryDir root;
    RideGroupJournal journal(root.path(), false);
    QVERIFY(journal.load().ok);
    QDir parent(root.path() + "/scootui");
    QVERIFY(parent.rename("ride-group", "retired"));
    QVERIFY(parent.mkdir("ride-group"));
    QVERIFY(!journal.save(record("idle")));
  }
  void exhaustedRecoveryFailsClosed() {
    QTemporaryDir root;
    {
      RideGroupJournal journal(root.path(), false);
      QVERIFY(journal.load().ok);
    }
    auto exhausted = record("preparing");
    exhausted["revision"] = 9007199254740991.0;
    QFile file(root.path() + "/scootui/ride-group/stop.json");
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    file.write(QJsonDocument(exhausted).toJson(QJsonDocument::Compact));
    file.close();
    RideGroupJournal journal(root.path(), false);
    QVERIFY(!journal.load().ok);
    QVERIFY(!journal.save(record("idle")));
  }
  void ownerReplacementFailsClosed() {
    QTemporaryDir root;
    RideGroupJournal journal(root.path(), false);
    QVERIFY(journal.load().ok);
    QString lock = root.path() + "/scootui/ride-group/owner.lock";
    QVERIFY(QFile::remove(lock));
    QFile replacement(lock);
    QVERIFY(replacement.open(QIODevice::WriteOnly));
    replacement.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    replacement.close();
    QVERIFY(!journal.save(record("idle")));
  }
  void interruptedWritesRequireResynchronization() {
    for (int stage = 1; stage <= 5; ++stage) {
      QTemporaryDir root;
      FaultJournal journal(root.path());
      QVERIFY(journal.load().ok);
      QVERIFY(journal.save(record("preparing")));
      journal.calls = 0;
      journal.faultAt = stage;
      auto prepared = record("prepared", 2);
      prepared["expected_generation"] = 7;
      QVERIFY(!journal.save(prepared));
      QVERIFY(!journal.save(prepared));
      journal.faultAt = 0;
      auto recovered = journal.load();
      QVERIFY(recovered.ok);
      QVERIFY(recovered.record["blocked"].toBool());
      QCOMPARE(recovered.record["phase"].toString(),
               stage == 5 ? QString("prepared") : QString("uncertain"));
      QCOMPARE(recovered.record["revision"].toInt(), 2);
      if (stage == 5)
        QCOMPARE(recovered.record["expected_generation"].toInt(), 7);
      else
        QVERIFY(recovered.record["expected_generation"].isNull());
    }
  }
  void strictJson() {
    QJsonObject parsed;
    QVERIFY(RideGroupJson::parse("{\"v\":2,\"lat\":-0.0}", parsed));
    QVERIFY(!RideGroupJson::parse("{\"v\":2.0}", parsed));
    QVERIFY(!RideGroupJson::parse("{\"v\":2,\"\\u0076\":2}", parsed));
    QVERIFY(!RideGroupJson::parse("{\"x\":\"\\ud800\"}", parsed));
    QVERIFY(!RideGroupJson::parse("{\"x\":\"\\udc00\"}", parsed));
    QVERIFY(RideGroupJson::parse("{\"x\":\"\\ud83d\\ude00\"}", parsed));
    QVERIFY(!RideGroupJson::parse(QByteArray("{\"x\":\"") + char(0xff) + "\"}",
                                  parsed));
  }
};
QTEST_GUILESS_MAIN(RideGroupJournalTest)
#include "RideGroupJournalTest.moc"
