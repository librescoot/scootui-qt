#include "ToastService.h"
#include <QDebug>

ToastService::ToastService(QObject *parent)
    : QObject(parent)
{
}

QVariantList ToastService::toasts() const
{
    QVariantList list;
    for (const auto &t : m_toasts) {
        QVariantMap m;
        m[QStringLiteral("id")] = t.id;
        m[QStringLiteral("message")] = t.message;
        m[QStringLiteral("type")] = t.type;
        m[QStringLiteral("permanent")] = t.permanent;
        list.append(m);
    }
    return list;
}

void ToastService::showInfo(const QString &message)
{
    addToast(message, QStringLiteral("info"), false);
}

void ToastService::showError(const QString &message)
{
    addToast(message, QStringLiteral("error"), false);
}

void ToastService::showWarning(const QString &message)
{
    addToast(message, QStringLiteral("warning"), false);
}

void ToastService::showSuccess(const QString &message)
{
    addToast(message, QStringLiteral("success"), false);
}

QString ToastService::showPermanentInfo(const QString &message, const QString &id)
{
    return addToast(message, QStringLiteral("info"), true, id);
}

QString ToastService::showPermanentError(const QString &message, const QString &id)
{
    return addToast(message, QStringLiteral("error"), true, id);
}

void ToastService::dismiss(const QString &id)
{
    for (int i = 0; i < m_toasts.size(); ++i) {
        if (m_toasts[i].id == id) {
            m_toasts.removeAt(i);
            emit toastsChanged();
            return;
        }
    }
}

QString ToastService::addToast(const QString &message, const QString &type, bool permanent, const QString &id)
{
    // If an id is provided and already exists, update it
    if (!id.isEmpty()) {
        for (auto &t : m_toasts) {
            if (t.id == id) {
                t.message = message;
                t.type = type;
                t.permanent = permanent;
                emit toastsChanged();
                return id;
            }
        }
    }

    ToastEntry entry;
    entry.id = id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : id;
    entry.message = message;
    entry.type = type;
    entry.permanent = permanent;
    m_toasts.append(entry);
    emit toastsChanged();

    if (!permanent) {
        scheduleRemoval(entry.id, type == QLatin1String("error") ? 5000 : 3000);
    }

    return entry.id;
}

void ToastService::scheduleRemoval(const QString &id, int ms)
{
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(ms);
    connect(timer, &QTimer::timeout, this, [this, id, timer]() {
        dismiss(id);
        timer->deleteLater();
    });
    timer->start();
}
