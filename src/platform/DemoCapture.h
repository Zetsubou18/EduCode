#pragma once
#include <QImage>
#include <QQuickWindow>
namespace DemoCapture {
void reset();
QImage capture(QQuickWindow *ide, qint64 processId);
} // namespace DemoCapture
