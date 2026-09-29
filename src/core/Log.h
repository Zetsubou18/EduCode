#pragma once
#include <QString>
namespace Log {
void captureStartup();
void initialize();
void write(const QString &category, const QString &message);
QString path();
} // namespace Log
