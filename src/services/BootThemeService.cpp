#include "BootThemeService.h"

#include "repositories/MdbRepository.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSoundEffect>
#include <QTimer>
#include <QUrl>

#include <algorithm>

namespace {

// Where the request and the DBC's answer live. Shared with lsc and
// dbc-dispatcher; the names are the interface.
const QString kDesiredKey = QStringLiteral("boot:desired");
const QString kStateKey = QStringLiteral("boot:dbc");
const QString kCommandChannel = QStringLiteral("boot:command");

const QString kThemeField = QStringLiteral("theme");
const QString kSoundField = QStringLiteral("sound");
const QString kNoteField = QStringLiteral("note");

// How often to re-read the published state while waiting for an answer, and
// how many times before deciding the DBC is not going to give one.
constexpr int kVerifyIntervalMs = 300;
constexpr int kVerifyTries = 8;

const QStringList &canonicalThemes()
{
    static const QStringList themes = {
        QStringLiteral("librescoot"),
        QStringLiteral("windowsxp"),
        QStringLiteral("librescoot-xp"),
        QStringLiteral("coopertino"),
    };
    return themes;
}

const QRegularExpression &themeNameRe()
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*$"));
    return re;
}

} // namespace

QString BootThemeService::assetDir()
{
    const QByteArray override = qgetenv("SCOOTUI_BOOT_ANIMATION_DIR");
    if (!override.isEmpty())
        return QString::fromLocal8Bit(override);
    return QStringLiteral("/usr/share/boot-animation");
}

QStringList BootThemeService::shippedThemes()
{
    return canonicalThemes();
}

QStringList BootThemeService::themesInDir(const QString &dir, bool *readable)
{
    QDir directory(dir);
    if (readable)
        *readable = directory.exists();

    QStringList found;
    const QFileInfoList entries = directory.entryInfoList(
        {QStringLiteral("*.json")}, QDir::Files | QDir::System);
    for (const QFileInfo &entry : entries) {
        const QString name = entry.completeBaseName();
        if (!themeNameRe().match(name).hasMatch() || found.contains(name))
            continue;
        found.append(name);
    }

    const QStringList &canonical = canonicalThemes();
    std::sort(found.begin(), found.end(), [&canonical](const QString &a, const QString &b) {
        const int rankA = canonical.indexOf(a);
        const int rankB = canonical.indexOf(b);
        if (rankA >= 0 && rankB >= 0) return rankA < rankB;
        if (rankA >= 0) return true;
        if (rankB >= 0) return false;
        return a < b;
    });
    return found;
}

bool BootThemeService::parseSoundValue(const QString &raw, bool *ok)
{
    if (ok)
        *ok = true;
    const QString value = raw.trimmed().toLower();
    if (value.isEmpty())
        return true; // unset is the normal state and means on
    if (value == QLatin1String("1") || value == QLatin1String("true")
        || value == QLatin1String("yes") || value == QLatin1String("on"))
        return true;
    if (value == QLatin1String("0") || value == QLatin1String("false")
        || value == QLatin1String("no") || value == QLatin1String("off"))
        return false;
    if (ok)
        *ok = false;
    return true;
}

BootThemeService::BootThemeService(QObject *parent)
    : QObject(parent)
{
    m_verifyTimer = new QTimer(this);
    m_verifyTimer->setInterval(kVerifyIntervalMs);
    connect(m_verifyTimer, &QTimer::timeout, this, &BootThemeService::verifyPending);
}

BootThemeService::~BootThemeService() = default;

void BootThemeService::setRepository(MdbRepository *repo)
{
    m_repo = repo;
    if (!m_repo)
        return;
    connect(m_repo, &MdbRepository::fieldsUpdated,
            this, &BootThemeService::onFieldsUpdated);
    connect(m_repo, &MdbRepository::connectionStateChanged, this, [this](bool connected) {
        // The state goes stale across a disconnect, so re-read it on return.
        if (connected)
            refresh();
    });
    refresh();
}

QStringList BootThemeService::themes() const
{
    bool readable = false;
    const QStringList installed = themesInDir(assetDir(), &readable);
    if (!readable || installed.isEmpty())
        return shippedThemes();
    return installed;
}

QString BootThemeService::displayName(const QString &theme) const
{
    if (theme == QLatin1String("librescoot")) return QStringLiteral("Librescoot");
    if (theme == QLatin1String("windowsxp")) return QStringLiteral("Windows XP");
    if (theme == QLatin1String("librescoot-xp")) return QStringLiteral("Librescoot XP");
    if (theme == QLatin1String("coopertino")) return QStringLiteral("Coopertino");

    QString pretty = theme;
    pretty.replace(QLatin1Char('-'), QLatin1Char(' '));
    pretty.replace(QLatin1Char('_'), QLatin1Char(' '));
    const QStringList words = pretty.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList capitalised;
    for (const QString &word : words)
        capitalised.append(word.left(1).toUpper() + word.mid(1));
    return capitalised.join(QLatin1Char(' '));
}

QString BootThemeService::soundFileName(const QString &theme) const
{
    // The stock theme's sound lives in the boot-animation repository rather
    // than in this layer, so it keeps its original name.
    if (theme == QLatin1String("librescoot")) return QStringLiteral("scooter-unlock.wav");
    if (theme == QLatin1String("windowsxp")) return QStringLiteral("windowsxp.wav");
    if (theme == QLatin1String("librescoot-xp")) return QStringLiteral("librescoot-xp.wav");
    if (theme == QLatin1String("coopertino")) return QStringLiteral("coopertino.wav");
    return {};
}

void BootThemeService::refresh()
{
    if (m_repo)
        m_repo->requestAll(kStateKey);
}

void BootThemeService::onFieldsUpdated(const QString &channel, const FieldMap &fields)
{
    if (channel != kStateKey)
        return;

    const QString theme = fields.value(kThemeField).trimmed();
    const QString sound = fields.value(kSoundField).trimmed();
    const QString note = fields.value(kNoteField).trimmed();
    const qint64 updated = fields.value(QStringLiteral("updated")).toLongLong();

    if (!theme.isEmpty() && theme != m_theme) {
        m_theme = theme;
        emit themeChanged();
    }
    bool ok = true;
    const bool enabled = parseSoundValue(sound, &ok);
    if (ok && enabled != m_soundEnabled) {
        m_soundEnabled = enabled;
        emit soundChanged();
    }

    settlePending(theme, sound, note, updated);
}

bool BootThemeService::request(const QString &field, const QString &value)
{
    if (!m_repo)
        return false;
    const bool isTheme = field == kThemeField;
    if (isTheme && !themeNameRe().match(value).hasMatch())
        return false;
    if (!isTheme && value != QLatin1String("0") && value != QLatin1String("1"))
        return false;

    // Desired state first: if this process died between the two calls, the
    // request would still be there for the DBC to pick up. The write's own
    // publish is suppressed — the command channel is the announcement, and
    // publishing on the hash's name would be noise.
    m_repo->set(kDesiredKey, field, value, /*publish=*/false);
    m_repo->publish(kCommandChannel,
                    (isTheme ? QStringLiteral("boot-theme ") : QStringLiteral("boot-sound ")) + value);

    m_pendingField = field;
    m_pendingValue = value;
    m_pendingSentAt = QDateTime::currentSecsSinceEpoch();
    m_verifyTries = 0;
    m_verifyTimer->start();
    return true;
}

bool BootThemeService::setTheme(const QString &theme)
{
    return request(kThemeField, theme);
}

bool BootThemeService::setSoundEnabled(bool enabled)
{
    return request(kSoundField, enabled ? QStringLiteral("1") : QStringLiteral("0"));
}

void BootThemeService::verifyPending()
{
    if (m_pendingField.isEmpty()) {
        m_verifyTimer->stop();
        return;
    }
    if (++m_verifyTries > kVerifyTries) {
        // No answer: the DBC is asleep and will apply this when it starts.
        // That is not a failure, so say nothing.
        m_pendingField.clear();
        m_pendingValue.clear();
        m_verifyTimer->stop();
        return;
    }
    refresh();
}

void BootThemeService::settlePending(const QString &theme, const QString &sound,
                                     const QString &note, qint64 updated)
{
    if (m_pendingField.isEmpty() || updated < m_pendingSentAt)
        return;

    const QString actual = m_pendingField == kThemeField ? theme : sound;
    const bool fresh = note.isEmpty() || note == QStringLiteral("ok");

    if (actual == m_pendingValue && fresh) {
        m_pendingField.clear();
        m_pendingValue.clear();
        m_verifyTimer->stop();
        return;
    }
    if (actual != m_pendingValue && !fresh) {
        const QString reason = explainNote(note);
        m_pendingField.clear();
        m_pendingValue.clear();
        m_verifyTimer->stop();
        emit applyFailed(reason);
    }
}

QString BootThemeService::explainNote(const QString &note)
{
    if (note == QLatin1String("unknown-theme"))
        return QStringLiteral("that theme is not installed on the DBC");
    if (note == QLatin1String("bad-sound"))
        return QStringLiteral("the DBC rejected the sound value");
    if (note == QLatin1String("env-error"))
        return QStringLiteral("the DBC could not write its U-Boot environment");
    return note;
}

void BootThemeService::playSound(const QString &theme)
{
    const QString file = soundFileName(theme);
    if (file.isEmpty())
        return;
    const QString path = assetDir() + QLatin1Char('/') + file;
    if (!QFileInfo::exists(path)) {
        qDebug() << "BootTheme: no startup sound at" << path;
        return;
    }

    if (!m_effect) {
        m_effect = new QSoundEffect(this);
        m_effect->setVolume(0.9);
        connect(m_effect, &QSoundEffect::statusChanged, this, [this]() {
            // A local file loads asynchronously; play once it is ready
            // instead of dropping the first request.
            if (m_effect->status() == QSoundEffect::Ready && !m_pendingPlay.isEmpty()) {
                m_pendingPlay.clear();
                m_effect->play();
            }
        });
    }

    m_pendingPlay = path;
    m_effect->setSource(QUrl::fromLocalFile(path));
    if (m_effect->status() == QSoundEffect::Ready) {
        m_pendingPlay.clear();
        m_effect->play();
    }
}