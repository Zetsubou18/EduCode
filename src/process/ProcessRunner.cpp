#include "ProcessRunner.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
ProcessRunner::ProcessRunner(QObject *p) : QObject(p) {
    process.setProcessChannelMode(QProcess::MergedChannels);
    timer.setInterval(33);
    connect(&timer, &QTimer::timeout, this, &ProcessRunner::flush);
    connect(&process, &QProcess::readyReadStandardOutput, this, &ProcessRunner::drain);
    connect(&process, &QProcess::stateChanged, this, [this] { emit runningChanged(); });
    connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
                drain();
                flush();
                timer.stop();
                if (!systemOutput)
                    emit output(QStringLiteral("\r\nПроцесс завершён · код %1%2\r\n")
                                    .arg(code)
                                    .arg(status == QProcess::CrashExit ? QStringLiteral(" · остановлен")
                                                                       : QString()));
            });
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            timer.stop();
            emit output(process.errorString() + "\r\n");
        }
    });
}
ProcessRunner::~ProcessRunner() {
    process.kill();
    process.waitForFinished(1500);
}
void ProcessRunner::run(const QString &python, const QString &file, const QString &dir, bool systemConsole) {
    if (running())
        return;
    systemOutput = systemConsole;
    pending.clear();
    decoder.reset(QTextCodec::codecForName("UTF-8")->makeDecoder());
    auto env = QProcessEnvironment::systemEnvironment();
#ifdef Q_OS_LINUX
    // Window capture uses isolated XWayland surfaces, never a whole-desktop portal.
    if (demoMode) {
        env.insert("QT_QPA_PLATFORM", "xcb");
        env.insert("GDK_BACKEND", "x11");
        env.insert("SDL_VIDEODRIVER", "x11");
    }
#endif
    env.insert("PYTHONIOENCODING", "utf-8");
    env.insert("PYTHONUNBUFFERED", "1");
    const auto helper = qEnvironmentVariable("EDUCODE_PYTHON_TOOLS");
    if (!helper.isEmpty())
        env.insert("PYTHONPATH", helper + QDir::listSeparator() + env.value("PYTHONPATH"));
    process.setProcessEnvironment(env);
    process.setWorkingDirectory(dir);
    QString program = python;
    QStringList arguments{"-u", file};
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier([systemConsole](QProcess::CreateProcessArguments *args) {
        if (systemConsole) {
            args->flags |= CREATE_NEW_CONSOLE;
            args->flags &= ~CREATE_NO_WINDOW;
            args->startupInfo->dwFlags &= ~STARTF_USESTDHANDLES;
            args->startupInfo->dwFlags &= ~STARTF_USESHOWWINDOW;
        }
    });
    if (systemConsole)
        arguments = QStringList{"-u", QDir(helper).filePath("run-system.py"), file};
#else
    if (systemConsole) {
        QString emulator;
        for (const auto &name : QStringList{"x-terminal-emulator", "gnome-terminal", "konsole", "xterm"}) {
            emulator = QStandardPaths::findExecutable(name);
            if (!emulator.isEmpty())
                break;
        }
        if (emulator.isEmpty()) {
            emit output("Не найден системный терминал. Выберите консоль IDE.\r\n");
            return;
        }
        program = emulator;
        arguments = QStringList{"-e", python, "-u", QDir(helper).filePath("run-system.py"), file};
        if (QFileInfo(emulator).fileName() == "gnome-terminal")
            arguments = QStringList{"--wait", "--", python, "-u", QDir(helper).filePath("run-system.py"), file};
        if (QFileInfo(emulator).fileName() == "konsole")
            arguments.prepend("--nofork");
    }
#endif
    if (!systemConsole)
        emit output("\x1b[2J\x1b[H" + QFileInfo(file).fileName() + "\r\n\r\n");
    timer.start();
    process.start(program, arguments);
}
void ProcessRunner::drain() {
    QByteArray bytes = process.readAllStandardOutput();
    if (decoder && !systemOutput)
        pending += decoder->toUnicode(bytes);
    if (pending.size() > 256 * 1024)
        flush();
}
void ProcessRunner::flush() {
    if (pending.isEmpty())
        return;
    QString text;
    pending.swap(text);
    text.replace("\r\n", "\n");
    text.replace("\n", "\r\n");
    emit output(text);
}
void ProcessRunner::input(const QString &text) {
    if (running())
        process.write(text.toUtf8());
}
void ProcessRunner::stop() {
    if (running())
        process.kill();
}
