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

    // Type scale (constant — not theme-dependent)
    Q_PROPERTY(qreal fontDisplay MEMBER s_fontDisplay CONSTANT)
    Q_PROPERTY(qreal fontPin MEMBER s_fontPin CONSTANT)
    Q_PROPERTY(qreal fontHero MEMBER s_fontHero CONSTANT)
    Q_PROPERTY(qreal fontXL MEMBER s_fontXL CONSTANT)
    Q_PROPERTY(qreal fontFeature MEMBER s_fontFeature CONSTANT)
    Q_PROPERTY(qreal fontInput MEMBER s_fontInput CONSTANT)
    Q_PROPERTY(qreal fontHeading MEMBER s_fontHeading CONSTANT)
    Q_PROPERTY(qreal fontTitle MEMBER s_fontTitle CONSTANT)
    Q_PROPERTY(qreal fontBody MEMBER s_fontBody CONSTANT)
    Q_PROPERTY(qreal fontCaption MEMBER s_fontCaption CONSTANT)
    Q_PROPERTY(qreal fontMicro MEMBER s_fontMicro CONSTANT)

    // Border radii (constant)
    Q_PROPERTY(qreal radiusBar MEMBER s_radiusBar CONSTANT)
    Q_PROPERTY(qreal radiusCard MEMBER s_radiusCard CONSTANT)
    Q_PROPERTY(qreal radiusModal MEMBER s_radiusModal CONSTANT)

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

    // Semantic status colors (theme-independent — deep shades, white text on top)
    Q_PROPERTY(QColor statusSuccess READ statusSuccess CONSTANT)
    Q_PROPERTY(QColor statusWarning READ statusWarning CONSTANT)
    Q_PROPERTY(QColor statusError   READ statusError   CONSTANT)
    Q_PROPERTY(QColor statusNeutral READ statusNeutral CONSTANT)
    Q_PROPERTY(QColor statusInfo    READ statusInfo    CONSTANT)

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

    // Material 800/900 — readable with white text in both themes
    QColor statusSuccess() const { return QColor(0x2E,0x7D,0x32); }  // Green 800
    QColor statusWarning() const { return QColor(0xE6,0x51,0x00); }  // Orange 900
    QColor statusError()   const { return QColor(0xC6,0x28,0x28); }  // Red 800
    QColor statusNeutral() const { return QColor(0x42,0x42,0x42); }  // Grey 800
    QColor statusInfo()    const { return QColor(0x15,0x65,0xC0); }  // Blue 800

    Q_INVOKABLE void setTheme(const QString &theme);

signals:
    void themeChanged();

private slots:
    void onSettingsThemeChanged();

private:
    SettingsStore *m_settings;
    bool m_isDark = true;
    bool m_isAutoMode = false;

    // Type scale constants
    static constexpr qreal s_fontDisplay = 96;
    static constexpr qreal s_fontPin     = 80;
    static constexpr qreal s_fontHero    = 64;
    static constexpr qreal s_fontXL      = 48;
    static constexpr qreal s_fontFeature = 36;
    static constexpr qreal s_fontInput   = 32;
    static constexpr qreal s_fontHeading = 28;
    static constexpr qreal s_fontTitle   = 20;
    static constexpr qreal s_fontBody    = 18;
    static constexpr qreal s_fontCaption = 14;
    static constexpr qreal s_fontMicro   = 10;

    // Border radii constants
    static constexpr qreal s_radiusBar   = 2;
    static constexpr qreal s_radiusCard  = 8;
    static constexpr qreal s_radiusModal = 16;
};
