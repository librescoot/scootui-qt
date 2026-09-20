#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "repositories/MdbRepository.h"

class QSoundEffect;
class QTimer;

// The DBC's boot theme and startup sound, driven through the same Redis
// interface lsc uses: a request is written to boot:desired and announced on
// boot:command, and the DBC's dbc-dispatcher is what actually writes the
// U-Boot environment and publishes the result to boot:dbc.
//
// This service therefore never touches fw_printenv/fw_setenv. That matters
// beyond tidiness: while the easter-egg menu wrote the environment directly,
// dbc-dispatcher would re-apply the requested value over it at the next start
// and the rider's choice would silently revert. One writer, one source of
// truth.
//
// Reads come from the published state, so they show what the environment
// actually holds rather than what was asked for.
class BootThemeService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(bool soundEnabled READ soundEnabled NOTIFY soundChanged)
    Q_PROPERTY(QStringList themes READ themes NOTIFY themesChanged)

public:
    // Where boot-animation installs its JSON, stream and sound triplets.
    // Overridable so a simulator or a dev build can point somewhere real.
    static QString assetDir();

    explicit BootThemeService(QObject *parent = nullptr);
    ~BootThemeService() override;

    // The service cannot do anything until it has the datastore.
    void setRepository(MdbRepository *repo);

    QString theme() const { return m_theme; }
    bool soundEnabled() const { return m_soundEnabled; }
    QStringList themes() const;

    // Pretty name for a theme id, e.g. "librescoot-xp" -> "Librescoot XP".
    Q_INVOKABLE QString displayName(const QString &theme) const;
    // The startup sound file for a theme, or an empty string if it has none.
    Q_INVOKABLE QString soundFileName(const QString &theme) const;

    // Ask for the current published state. Cheap, and safe to call whenever
    // the menu is opened.
    Q_INVOKABLE void refresh();

    // Both return false only when nothing could be requested at all (no
    // datastore, or an unusable value). The outcome arrives later: a refusal
    // from the DBC is reported as applyFailed(), and an answer that never
    // comes simply means the DBC is asleep and will apply it when it starts.
    Q_INVOKABLE bool setTheme(const QString &theme);
    Q_INVOKABLE bool setSoundEnabled(bool enabled);

    // Plays a theme's startup sound. The real thing plays from the boot
    // launcher before the dashboard starts; this is only the menu's test.
    Q_INVOKABLE void playSound(const QString &theme);

    static QStringList shippedThemes();
    static QStringList themesInDir(const QString &dir, bool *readable = nullptr);
    // Interprets a boot_sound value. Absent or unreadable means on; *ok is
    // false only for a value that is neither on nor off.
    static bool parseSoundValue(const QString &raw, bool *ok = nullptr);

signals:
    void themeChanged();
    void soundChanged();
    void themesChanged();
    void applyFailed(const QString &reason);

private:
    void onFieldsUpdated(const QString &channel, const FieldMap &fields);
    bool request(const QString &field, const QString &value);
    void verifyPending();
    void settlePending(const QString &theme, const QString &sound, const QString &note,
                       qint64 updated);
    static QString explainNote(const QString &note);

    MdbRepository *m_repo = nullptr;
    QSoundEffect *m_effect = nullptr;
    QTimer *m_verifyTimer = nullptr;
    QString m_pendingPlay;
    // The request whose answer we are still waiting for, and when it went out.
    // The state's own timestamp has to be newer than this before its note is
    // read as our answer, or a refusal left over from an earlier attempt would
    // be reported as the verdict on this one.
    QString m_pendingField;
    QString m_pendingValue;
    qint64 m_pendingSentAt = 0;
    int m_verifyTries = 0;
    QString m_theme = QStringLiteral("librescoot");
    bool m_soundEnabled = true;
};