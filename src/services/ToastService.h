#pragma once

#include <QObject>
#include <QVariantList>
#include <QTimer>
#include <QUuid>
#include <QtQml/qqmlregistration.h>

struct ToastEntry {
    QString id;
    QString message;
    QString type;      // "info", "error", "warning", "success"
    bool permanent;
};

class QQmlEngine;
class QJSEngine;

class ToastService : public QObject
{
    Q_OBJECT
    QML_SINGLETON
    QML_ELEMENT
    Q_PROPERTY(QVariantList toasts READ toasts NOTIFY toastsChanged)

public:
    explicit ToastService(QObject *parent = nullptr);
    static ToastService *create(QQmlEngine *, QJSEngine *) { return s_instance; }

    QVariantList toasts() const;

    Q_INVOKABLE void showInfo(const QString &message);
    Q_INVOKABLE void showError(const QString &message);
    Q_INVOKABLE void showWarning(const QString &message);
    Q_INVOKABLE void showSuccess(const QString &message);
    Q_INVOKABLE QString showPermanentInfo(const QString &message, const QString &id = {});
    Q_INVOKABLE QString showPermanentError(const QString &message, const QString &id = {});
    Q_INVOKABLE QString showPermanentWarning(const QString &message, const QString &id = {});
    Q_INVOKABLE void dismiss(const QString &id);

signals:
    void toastsChanged();

private:
    QString addToast(const QString &message, const QString &type, bool permanent, const QString &id = {});
    void scheduleRemoval(const QString &id, int ms);

    QList<ToastEntry> m_toasts;
    static inline ToastService *s_instance = nullptr;
};
