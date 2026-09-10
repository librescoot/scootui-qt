#pragma once

#include "HmiLatencyMetrics.h"
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <memory>

class QQuickWindow;

class HmiLatencyMonitor : public QObject
{
    Q_OBJECT
public:
    explicit HmiLatencyMonitor(QObject *parent = nullptr);
    void attachWindow(QQuickWindow *window);
    void stop();
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    static qint64 nowMs();
    void sample();
    QTimer m_timer;
    HmiGuiLatencyMetrics m_gui;
    QPointer<QQuickWindow> m_window;
    std::shared_ptr<HmiFrameMetrics> m_frames = std::make_shared<HmiFrameMetrics>();
};
