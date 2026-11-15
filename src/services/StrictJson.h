#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

// QJsonDocument keeps only the last occurrence of a key. Check decoded keys
// before parsing so capability requirements cannot be replaced by duplicates.
class StrictJson
{
public:
    static bool parseObject(const QByteArray &input, QJsonObject *out)
    {
        if (!out || input.size() > 16 * 1024 * 1024) return false;
        StrictJson check(input);
        if (!check.value(0)) return false;
        check.whitespace();
        if (check.pos != input.size()) return false;
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(input, &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) return false;
        *out = doc.object();
        return true;
    }

private:
    explicit StrictJson(const QByteArray &bytes) : data(bytes) {}
    void whitespace() {
        while (pos < data.size() && (data[pos] == ' ' || data[pos] == '\n'
               || data[pos] == '\r' || data[pos] == '\t')) ++pos;
    }
    bool string(QString *decoded = nullptr) {
        if (pos >= data.size() || data[pos] != '"') return false;
        const qsizetype start = pos++;
        bool escaped = false;
        while (pos < data.size()) {
            const char c = data[pos++];
            if (escaped) { escaped = false; continue; }
            if (c == '\\') { escaped = true; continue; }
            if (c == '"') {
                if (!decoded) return true;
                QJsonParseError error;
                const auto wrapper = QByteArray("[") + data.mid(start, pos - start) + "]";
                const auto doc = QJsonDocument::fromJson(wrapper, &error);
                if (error.error != QJsonParseError::NoError || !doc.isArray()
                    || doc.array().size() != 1 || !doc.array().at(0).isString()) return false;
                *decoded = doc.array().at(0).toString();
                return true;
            }
        }
        return false;
    }
    bool value(int depth) {
        if (depth > 100) return false;
        whitespace();
        if (pos >= data.size()) return false;
        if (data[pos] == '"') return string();
        if (data[pos] == '{') {
            ++pos; whitespace(); QSet<QString> keys;
            if (pos < data.size() && data[pos] == '}') { ++pos; return true; }
            while (pos < data.size()) {
                whitespace(); QString key;
                if (!string(&key) || keys.contains(key)) return false;
                keys.insert(key); whitespace();
                if (pos >= data.size() || data[pos++] != ':') return false;
                if (!value(depth + 1)) return false;
                whitespace();
                if (pos >= data.size()) return false;
                const char c = data[pos++];
                if (c == '}') return true;
                if (c != ',') return false;
            }
            return false;
        }
        if (data[pos] == '[') {
            ++pos; whitespace();
            if (pos < data.size() && data[pos] == ']') { ++pos; return true; }
            while (pos < data.size()) {
                if (!value(depth + 1)) return false;
                whitespace();
                if (pos >= data.size()) return false;
                const char c = data[pos++];
                if (c == ']') return true;
                if (c != ',') return false;
            }
            return false;
        }
        const qsizetype start = pos;
        while (pos < data.size() && data[pos] != ',' && data[pos] != ']'
               && data[pos] != '}' && data[pos] != ' ' && data[pos] != '\n'
               && data[pos] != '\r' && data[pos] != '\t') ++pos;
        return pos > start;
    }
    const QByteArray &data;
    qsizetype pos = 0;
};
