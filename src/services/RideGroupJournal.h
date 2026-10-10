#pragma once
#include <QJsonObject>
#include <QMutex>
#include <QString>

struct RideGroupJournalResult {
  bool ok = false;
  bool missing = false;
  QJsonObject record;
};

// The production root is /data. Tests inject an isolated storage root.
class RideGroupJournal {
public:
  explicit RideGroupJournal(QString root = QStringLiteral("/data"),
                            bool requireMount = true);
  virtual ~RideGroupJournal();
  RideGroupJournalResult load();
  bool save(const QJsonObject &record);
  static bool valid(const QJsonObject &record);

protected:
  enum class WriteStage { Written, FileSync, Close, Rename, DirectorySync };
  virtual bool checkpoint(WriteStage) { return true; }

private:
  bool openStorage();
  QString m_root;
  bool m_requireMount;
  int m_directory = -1;
  int m_owner = -1;
  QMutex m_mutex;
  bool m_loaded = false;
  bool m_faulted = false;
  QJsonObject m_record;
};
