#pragma once

#include <QObject>
#include <QColor>

class SettingsStore;

class ThemeStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isDark READ isDark NOTIFY themeChanged)
    Q_PROPERTY(QString themeName READ themeName NOTIFY themeChanged)
    Q_PROPERTY(bool isAutoMode READ isAutoMode NOTIFY themeChanged)

    // Colors
    Q_PROPERTY(QColor textColor READ textColor NOTIFY themeChanged)
    Q_PROPERTY(QColor textSecondary READ textSecondary NOTIFY themeChanged)
    Q_PROPERTY(QColor textTertiary READ textTertiary NOTIFY themeChanged)
    Q_PROPERTY(QColor textHint READ textHint NOTIFY themeChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY themeChanged)
    Q_PROPERTY(QColor surfaceColor READ surfaceColor NOTIFY themeChanged)
    Q_PROPERTY(QColor borderColor READ borderColor NOTIFY themeChanged)
    Q_PROPERTY(QColor arcBackground READ arcBackground NOTIFY themeChanged)
    Q_PROPERTY(QColor powerBarBg READ powerBarBg NOTIFY themeChanged)
    Q_PROPERTY(QColor powerZeroMark READ powerZeroMark NOTIFY themeChanged)

public:
    explicit ThemeStore(SettingsStore *settings, QObject *parent = nullptr);

    bool isDark() const { return m_isDark; }
    bool isAutoMode() const { return m_isAutoMode; }
    QString themeName() const { return m_isDark ? QStringLiteral("dark") : QStringLiteral("light"); }

    QColor textColor() const { return m_isDark ? QColor(255,255,255) : QColor(0,0,0); }
    QColor textSecondary() const { return m_isDark ? QColor(255,255,255,153) : QColor(0,0,0,138); }
    QColor textTertiary() const { return m_isDark ? QColor(255,255,255,77) : QColor(0,0,0,31); }
    QColor textHint() const { return m_isDark ? QColor(255,255,255,138) : QColor(0,0,0,97); }
    QColor backgroundColor() const { return m_isDark ? QColor(0,0,0) : QColor(255,255,255); }
    QColor surfaceColor() const { return m_isDark ? QColor(0x1E,0x1E,0x1E) : QColor(0xF5,0xF5,0xF5); }
    QColor borderColor() const { return m_isDark ? QColor(255,255,255,26) : QColor(0,0,0,31); }
    QColor arcBackground() const { return m_isDark ? QColor(0x42,0x42,0x42) : QColor(0xE0,0xE0,0xE0); }
    QColor powerBarBg() const { return m_isDark ? QColor(0x42,0x42,0x42) : QColor(0xE0,0xE0,0xE0); }
    QColor powerZeroMark() const { return m_isDark ? QColor(255,255,255,102) : QColor(0,0,0,97); }

    Q_INVOKABLE void setTheme(const QString &theme);

signals:
    void themeChanged();

private slots:
    void onSettingsThemeChanged();

private:
    SettingsStore *m_settings;
    bool m_isDark = true;
    bool m_isAutoMode = false;
};
