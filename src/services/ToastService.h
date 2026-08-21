#pragma once

#include <QObject>
#include <QVariantList>
#include <QTimer>
#include <QUuid>
#include <QtQml/qqmlengine.h>

struct ToastEntry {
    QString id;
    QString message;
    QString type;      // "info", "error", "warning", "success"
    bool permanent;
    QString icon;      // optional qrc icon path, empty = no icon
};

class ToastService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantList toasts READ toasts NOTIFY toastsChanged)

public:
    explicit ToastService(QObject *parent = nullptr);

    QVariantList toasts() const;

    Q_INVOKABLE void showInfo(const QString &message);
    Q_INVOKABLE void showError(const QString &message);
    Q_INVOKABLE void showWarning(const QString &message);
    Q_INVOKABLE void showSuccess(const QString &message);
    Q_INVOKABLE QString showPermanentInfo(const QString &message, const QString &id = {}, const QString &icon = {});
    Q_INVOKABLE QString showPermanentError(const QString &message, const QString &id = {}, const QString &icon = {});
    Q_INVOKABLE QString showPermanentWarning(const QString &message, const QString &id = {}, const QString &icon = {});
    Q_INVOKABLE void dismiss(const QString &id);

signals:
    void toastsChanged();

private:
    QString addToast(const QString &message, const QString &type, bool permanent, const QString &id = {}, const QString &icon = {});
    void scheduleRemoval(const QString &id, int ms);

    QList<ToastEntry> m_toasts;

public:
    // Application owns the instance and wires its dependencies before the engine
    // loads. create() hands QML that object instead of a default-constructed one.
    static ToastService *create(QQmlEngine *, QJSEngine *)
    {
        Q_ASSERT(s_qmlInstance);
        QJSEngine::setObjectOwnership(s_qmlInstance, QJSEngine::CppOwnership);
        return s_qmlInstance;
    }
    static void setQmlInstance(ToastService *instance) { s_qmlInstance = instance; }

private:
    static inline ToastService *s_qmlInstance = nullptr;
};
