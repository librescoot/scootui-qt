#include "RideGroupJournal.h"
#include "RideGroupJson.h"
#include <QFile>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QStorageInfo>
#include <QUuid>
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
bool owned(int fd, bool directory, bool exactMode) {
  struct stat s {};
  return fstat(fd, &s) == 0 && s.st_uid == geteuid() && s.st_gid == getegid() &&
         (directory ? S_ISDIR(s.st_mode) : S_ISREG(s.st_mode)) &&
         (exactMode ? (s.st_mode & 0777) == (directory ? 0700 : 0600)
                    : (s.st_mode & 022) == 0) &&
         (directory || s.st_nlink == 1);
}
int childDirectory(int parent, const char *name, bool exact) {
  bool made = mkdirat(parent, name, 0700) == 0;
  if (!made && errno != EEXIST)
    return -1;
  int fd =
      openat(parent, name, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0)
    return -1;
  if (!owned(fd, true, exact) ||
      (made && (fsync(fd) != 0 || fsync(parent) != 0))) {
    close(fd);
    return -1;
  }
  return fd;
}
} // namespace
RideGroupJournal::RideGroupJournal(QString root, bool requireMount)
    : m_root(std::move(root)), m_requireMount(requireMount) {}
RideGroupJournal::~RideGroupJournal() {
  if (m_owner >= 0)
    close(m_owner);
  if (m_directory >= 0)
    close(m_directory);
}
bool RideGroupJournal::valid(const QJsonObject &r) {
  if (!RideGroupJson::keys(r, {"format", "revision", "blocked", "operation_id",
                               "phase", "expected_generation",
                               "committed_generation"}) ||
      r["format"] != 1 || !RideGroupJson::counter(r["revision"], false) ||
      !r["blocked"].isBool() || !r["phase"].isString())
    return false;
  const auto op = r["operation_id"], expected = r["expected_generation"],
             committed = r["committed_generation"];
  if (!(op.isNull() || RideGroupJson::uuid(op)) ||
      !(expected.isNull() || RideGroupJson::counter(expected)) ||
      !(committed.isNull() || RideGroupJson::counter(committed, false)))
    return false;
  const auto phase = r["phase"].toString();
  const bool blocked = r["blocked"].toBool();
  if (phase == "idle")
    return !blocked && op.isNull() && expected.isNull() && committed.isNull();
  if (!blocked)
    return false;
  if (phase == "preparing")
    return RideGroupJson::uuid(op) && expected.isNull() && committed.isNull();
  if (phase == "prepared")
    return RideGroupJson::uuid(op) && RideGroupJson::counter(expected) &&
           committed.isNull();
  if (phase == "confirmed")
    return RideGroupJson::uuid(op) && RideGroupJson::counter(expected) &&
           RideGroupJson::counter(committed, false) &&
           committed.toDouble() > expected.toDouble();
  if (phase == "uncertain")
    return committed.isNull() && (expected.isNull() || RideGroupJson::uuid(op));
  return false;
}
bool RideGroupJournal::openStorage() {
  if (m_owner >= 0) {
    if (m_requireMount) {
      QStorageInfo storage(m_root);
      if (!storage.isValid() || !storage.isReady() || storage.isReadOnly() ||
          storage.rootPath() != m_root)
        return false;
    }
    int root = open(QFile::encodeName(m_root).constData(),
                    O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (root < 0)
      return false;
    int parent = openat(root, "scootui",
                        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    close(root);
    if (parent < 0)
      return false;
    int current = openat(parent, "ride-group",
                         O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    close(parent);
    if (current < 0)
      return false;
    struct stat active {
    }, saved{};
    bool same = owned(current, true, true) && fstat(current, &active) == 0 &&
                fstat(m_directory, &saved) == 0 &&
                active.st_dev == saved.st_dev && active.st_ino == saved.st_ino;
    int owner =
        openat(current, "owner.lock", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    same = same && owner >= 0 && owned(owner, false, true) &&
           fstat(owner, &active) == 0 && fstat(m_owner, &saved) == 0 &&
           active.st_dev == saved.st_dev && active.st_ino == saved.st_ino;
    if (owner >= 0)
      close(owner);
    close(current);
    return same;
  }
  if (m_requireMount) {
    QStorageInfo storage(m_root);
    if (m_root != "/data" || !storage.isValid() || !storage.isReady() ||
        storage.isReadOnly() || storage.rootPath() != m_root)
      return false;
  }
  int root = open(QFile::encodeName(m_root).constData(),
                  O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (root < 0)
    return false;
  if (!owned(root, true, false)) {
    close(root);
    return false;
  }
  int parent = childDirectory(root, "scootui", false);
  close(root);
  if (parent < 0)
    return false;
  int dir = childDirectory(parent, "ride-group", true);
  if (dir < 0) {
    close(parent);
    return false;
  }
  if (fsync(dir) != 0 || fsync(parent) != 0) {
    close(dir);
    close(parent);
    return false;
  }
  close(parent);
  int owner = openat(dir, "owner.lock",
                     O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
  if (owner < 0 || !owned(owner, false, true) ||
      flock(owner, LOCK_EX | LOCK_NB) != 0 || fsync(owner) != 0 ||
      fsync(dir) != 0) {
    if (owner >= 0)
      close(owner);
    close(dir);
    return false;
  }
  m_owner = owner;
  m_directory = dir;
  return true;
}
RideGroupJournalResult RideGroupJournal::load() {
  QMutexLocker guard(&m_mutex);
  m_faulted = true;
  if (!openStorage())
    return {};
  int fd = openat(m_directory, "stop.json", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0) {
    if (errno == ENOENT && !m_loaded) {
      m_loaded = true;
      m_faulted = false;
      return {true, true, {}};
    }
    return {};
  }
  struct stat stat {};
  if (!owned(fd, false, true) || fstat(fd, &stat) != 0 || stat.st_size <= 0 ||
      stat.st_size > 1024) {
    close(fd);
    return {};
  }
  QByteArray bytes;
  char buffer[1025];
  for (;;) {
    ssize_t n = read(fd, buffer, sizeof(buffer));
    if (n < 0 && errno == EINTR)
      continue;
    if (n < 0) {
      close(fd);
      return {};
    }
    if (n == 0)
      break;
    bytes.append(buffer, n);
    if (bytes.size() > 1024) {
      close(fd);
      return {};
    }
  }
  bool synced = fsync(fd) == 0;
  bool closed = close(fd) == 0;
  QJsonObject record;
  if (!synced || !closed || fsync(m_directory) != 0 ||
      !RideGroupJson::parse(bytes, record) || !valid(record))
    return {};
  if (m_loaded && !m_record.isEmpty() &&
      (record["revision"].toDouble() < m_record["revision"].toDouble() ||
       (record["revision"] == m_record["revision"] && record != m_record)))
    return {};
  m_loaded = true;
  m_faulted = false;
  m_record = record;
  // Recovery cannot reconstruct a live explicit-intent permit.
  if (record["phase"] == "preparing") {
    if (record["revision"].toDouble() >= 9007199254740991.0)
      return {};
    record["phase"] = "uncertain";
    record["revision"] = record["revision"].toDouble() + 1;
    guard.unlock();
    if (!save(record))
      return {};
    return {true, false, record};
  }
  return {true, false, record};
}
bool RideGroupJournal::save(const QJsonObject &record) {
  QMutexLocker guard(&m_mutex);
  if (!m_loaded || m_faulted || !valid(record) || !openStorage())
    return false;
  double previous = m_record.isEmpty() ? 0 : m_record["revision"].toDouble();
  if (previous >= 9007199254740991.0 ||
      record["revision"].toDouble() != previous + 1)
    return false;
  auto phase = record["phase"].toString();
  if (m_record.isEmpty() && phase != "idle" && phase != "preparing")
    return false;
  if (!m_record.isEmpty() &&
      record["operation_id"] != m_record["operation_id"] && phase != "idle" &&
      phase != "preparing")
    return false;
  if (m_record["phase"] == "confirmed" &&
      record["operation_id"] == m_record["operation_id"] &&
      (phase != "confirmed" ||
       record["committed_generation"] != m_record["committed_generation"]))
    return false;
  if (!m_record.isEmpty() &&
      record["operation_id"] == m_record["operation_id"] &&
      !m_record["expected_generation"].isNull() &&
      record["expected_generation"] != m_record["expected_generation"])
    return false;
  QByteArray bytes = QJsonDocument(record).toJson(QJsonDocument::Compact);
  if (bytes.size() > 1024)
    return false;
  QByteArray temp =
      ".stop-" + QUuid::createUuid().toString(QUuid::WithoutBraces).toLatin1() +
      ".tmp";
  int fd = openat(m_directory, temp.constData(),
                  O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
  if (fd < 0)
    return false;
  int offset = 0;
  bool ok = owned(fd, false, true);
  while (ok && offset < bytes.size()) {
    ssize_t n = write(fd, bytes.constData() + offset, bytes.size() - offset);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0) {
      ok = false;
      break;
    }
    offset += n;
  }
  if (ok)
    ok = fsync(fd) == 0;
  if (close(fd) != 0)
    ok = false;
  if (ok)
    ok = renameat(m_directory, temp.constData(), m_directory, "stop.json") == 0;
  if (ok)
    ok = fsync(m_directory) == 0;
  if (!ok) {
    m_faulted = true;
    unlinkat(m_directory, temp.constData(), 0);
  } else
    m_record = record;
  return ok;
}
