#include "Log.h"
#include <cstdio>
#include <QDateTime>
#include <QCoreApplication>
#include <QSysInfo>
#include <QThread>
#include <QUuid>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QSaveFile>
namespace {
QMutex mutex;
QString logPath;
QString autoLogPath;
QFile logFile;
QStringList startupMessages;
} // namespace
void Log::captureStartup() {
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &, const QString &text) {
        QMutexLocker lock(&mutex);
        if (startupMessages.size() < 100) startupMessages << text;
        if (type == QtFatalMsg || type == QtCriticalMsg) {
            const auto bytes = text.toUtf8();
            std::fwrite(bytes.constData(), 1, size_t(bytes.size()), stderr); std::fputc('\n', stderr);
        }
    });
}
QString Log::path() {
    return logPath;
}
QString Log::snapshotPath() { return autoLogPath; }
void Log::writeSnapshot(const QString &json) {
    QMutexLocker lock(&mutex);
    if (autoLogPath.isEmpty())
        return;
    QSaveFile file(autoLogPath);
    const auto bytes = (QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) + "\n" + json + "\n").toUtf8();
    if (file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size())
        file.commit();
}
void Log::write(const QString &category, const QString &message) {
    QMutexLocker lock(&mutex);
    if (logPath.isEmpty())
        return;
    if (logFile.size() > 10 * 1024 * 1024) {
        logFile.close();
        logPath += ".next";
        logFile.setFileName(logPath);
        logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Unbuffered);
    }
    auto line = QString("%1 [%2] [thread=%3] %4\n")
        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs), category)
        .arg(quintptr(QThread::currentThreadId()), 0, 16)
        .arg(QString(message).replace('\n', " ").left(4000)).toUtf8();
    logFile.write(line);
    // QFile is kept open; flush important records for crash diagnosis.
    if (category == "ERROR" || category == "FATAL" || category == "LIFECYCLE") logFile.flush();

}
void Log::initialize() {
    const auto dir =
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("logs");
    QDir().mkpath(dir);
    autoLogPath = QDir(dir).filePath("AutoEDI.log");
    logPath = QDir(dir).filePath("ide-" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")
        + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8) + ".log");
    logFile.setFileName(logPath);
    logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Unbuffered);
    auto old = QDir(dir).entryInfoList({"ide-*.log*"}, QDir::Files, QDir::Time);
    for (int i = 100; i < old.size(); ++i) QFile::remove(old[i].absoluteFilePath());
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &context, const QString &text) {
        const char *names[] = {"DEBUG", "WARN", "ERROR", "FATAL", "INFO"};
        Log::write(names[qBound(0, int(type), 4)], QString("%1:%2 %3").arg(context.category ? context.category : "qt").arg(context.line).arg(text));
#ifndef Q_OS_WIN
        // Keep startup failures visible when launching from a Linux terminal.
        if (type == QtCriticalMsg || type == QtFatalMsg || qEnvironmentVariableIsSet("EDUCODE_VERBOSE")) {
            const auto bytes = text.toUtf8();
            std::fwrite(bytes.constData(), 1, size_t(bytes.size()), stderr);
            std::fputc('\n', stderr);
        }
#endif
        if (qEnvironmentVariableIsSet("EDUCODE_LOG")) {
            QFile file(qEnvironmentVariable("EDUCODE_LOG"));
            if (file.open(QIODevice::Append))
                file.write(text.toUtf8() + "\n");
        }
    });
    for (const auto &message : startupMessages) {
        write("STARTUP", message);
        if (qEnvironmentVariableIsSet("EDUCODE_VERBOSE")) {
            const auto bytes = message.toUtf8(); std::fwrite(bytes.constData(),1,size_t(bytes.size()),stderr);std::fputc('\n',stderr);
        }
    }
    startupMessages.clear();
    write("LIFECYCLE", "EduCode " + QCoreApplication::applicationVersion() + " started; OS=" + QSysInfo::prettyProductName() + "; CPU=" + QSysInfo::currentCpuArchitecture() + "; PID=" + QString::number(QCoreApplication::applicationPid()));
}
