#pragma once
#include <QString>
namespace Log {
void captureStartup();
void initialize();
void write(const QString &category, const QString &message);
void writeSnapshot(const QString &json);
QString path();
QString snapshotPath();
} // namespace Log
