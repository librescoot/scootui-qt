#include "RideGroupJson.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>
#include <cmath>

namespace {
const QSet<QString> integers = {"v",
                                "format",
                                "revision",
                                "journal_revision",
                                "consent_generation",
                                "expected_generation",
                                "committed_generation",
                                "stop_expected_generation",
                                "transport_epoch",
                                "source_epoch",
                                "seq",
                                "activity_seq",
                                "ttl_ms",
                                "captured_at_ms",
                                "age_ms",
                                "palette_index"};
class Scanner {
public:
  explicit Scanner(const QByteArray &data) : b(data) {}
  bool scan() {
    space();
    if (!value({}, 0))
      return false;
    space();
    return p == b.size();
  }

private:
  const QByteArray &b;
  int p = 0;
  void space() {
    while (p < b.size() &&
           (b[p] == ' ' || b[p] == '\n' || b[p] == '\r' || b[p] == '\t'))
      ++p;
  }
  bool hex(int &code) {
    if (p + 4 > b.size())
      return false;
    code = 0;
    for (int i = 0; i < 4; ++i) {
      char c = b[p++];
      int n = c >= '0' && c <= '9'   ? c - '0'
              : c >= 'a' && c <= 'f' ? c - 'a' + 10
              : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                     : -1;
      if (n < 0)
        return false;
      code = code * 16 + n;
    }
    return true;
  }
  bool string(QString *out = nullptr) {
    if (p >= b.size() || b[p] != '"')
      return false;
    int start = p++;
    while (p < b.size()) {
      unsigned char c = b[p++];
      if (c == '"') {
        if (out) {
          auto doc =
              QJsonDocument::fromJson("[" + b.mid(start, p - start) + "]");
          if (!doc.isArray())
            return false;
          *out = doc.array().at(0).toString();
        }
        return true;
      }
      if (c < 32)
        return false;
      if (c != '\\')
        continue;
      if (p >= b.size())
        return false;
      char e = b[p++];
      if (e == 'u') {
        int code;
        if (!hex(code))
          return false;
        if (code >= 0xd800 && code <= 0xdbff) {
          if (p + 2 > b.size() || b.mid(p, 2) != "\\u")
            return false;
          p += 2;
          int low;
          if (!hex(low) || low < 0xdc00 || low > 0xdfff)
            return false;
        } else if (code >= 0xdc00 && code <= 0xdfff)
          return false;
      } else if (!QByteArray("\"\\/bfnrt").contains(e))
        return false;
    }
    return false;
  }
  bool value(const QString &key, int depth) {
    if (depth > 12)
      return false;
    space();
    if (p >= b.size())
      return false;
    if (b[p] == '"')
      return string();
    if (b[p] == '{') {
      ++p;
      space();
      QSet<QString> seen;
      if (p < b.size() && b[p] == '}') {
        ++p;
        return true;
      }
      for (;;) {
        space();
        QString name;
        if (!string(&name) || seen.contains(name))
          return false;
        seen.insert(name);
        space();
        if (p >= b.size() || b[p++] != ':')
          return false;
        if (!value(name, depth + 1))
          return false;
        space();
        if (p >= b.size())
          return false;
        char end = b[p++];
        if (end == '}')
          return true;
        if (end != ',')
          return false;
      }
    }
    if (b[p] == '[') {
      ++p;
      space();
      if (p < b.size() && b[p] == ']') {
        ++p;
        return true;
      }
      for (;;) {
        if (!value({}, depth + 1))
          return false;
        space();
        if (p >= b.size())
          return false;
        char end = b[p++];
        if (end == ']')
          return true;
        if (end != ',')
          return false;
      }
    }
    int start = p;
    while (p < b.size() && !QByteArray(",]} \t\r\n").contains(b[p]))
      ++p;
    QByteArray token = b.mid(start, p - start);
    if (token == "true" || token == "false" || token == "null")
      return true;
    static const QRegularExpression number(
        QStringLiteral("^-?(0|[1-9][0-9]*)(\\.[0-9]+)?([eE][+-]?[0-9]+)?$"));
    if (!number.match(QString::fromLatin1(token)).hasMatch())
      return false;
    if (integers.contains(key) &&
        (token.contains('.') || token.contains('e') || token.contains('E')))
      return false;
    return true;
  }
};
} // namespace
bool RideGroupJson::parse(const QByteArray &bytes, QJsonObject &object) {
  if (bytes.isEmpty() || bytes.size() > 1024)
    return false;
  QStringDecoder decoder(QStringDecoder::Utf8);
  decoder.decode(bytes);
  if (decoder.hasError() || !Scanner(bytes).scan())
    return false;
  auto document = QJsonDocument::fromJson(bytes);
  if (!document.isObject())
    return false;
  object = document.object();
  return true;
}
bool RideGroupJson::keys(const QJsonObject &object,
                         const QStringList &expected) {
  if (object.size() != expected.size())
    return false;
  for (const auto &key : expected)
    if (!object.contains(key))
      return false;
  return true;
}
bool RideGroupJson::counter(const QJsonValue &v, bool zero) {
  if (!v.isDouble())
    return false;
  double n = v.toDouble();
  return std::isfinite(n) && std::floor(n) == n && n >= (zero ? 0 : 1) &&
         n <= 9007199254740991.0;
}
bool RideGroupJson::uuid(const QJsonValue &v) {
  static const QRegularExpression pattern(QStringLiteral(
      "^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"));
  return v.isString() && pattern.match(v.toString()).hasMatch();
}
