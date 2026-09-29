#include "TerminalSession.h"
#include <QJsonDocument>
#include <QJsonObject>
TerminalSession::TerminalSession(QObject *p) : QObject(p) {
    timer.setInterval(33);
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, this, [this] {
        if (!pending.isEmpty()) {
            QString s;
            pending.swap(s);
            emit output(s);
        }
    });
    connect(&process, &QProcess::readyReadStandardOutput, this, [this] {
        buffer += process.readAllStandardOutput();
        int i;
        while ((i = buffer.indexOf('\n')) >= 0) {
            auto obj = QJsonDocument::fromJson(buffer.left(i)).object();
            buffer.remove(0, i + 1);
            if (obj.contains("data"))
                pending += obj["data"].toString();
            if (!pending.isEmpty() && !timer.isActive()) timer.start();
            if (obj.contains("error"))
                emit error(obj["error"].toString());
        }
    });
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            emit error("Terminal: " + process.errorString());
    });
    connect(&process, &QProcess::readyReadStandardError, this,
            [this] { emit error(QString::fromUtf8(process.readAllStandardError()).left(1500)); });
}
TerminalSession::~TerminalSession() {
    stop();
}
void TerminalSession::start(const QString &n, const QString &s, const QString &root, const QString &venv) {
    if (process.state() != QProcess::NotRunning)
        return;
    process.start(n, {s, root, venv});
}
void TerminalSession::stop() {
    timer.stop();
    process.kill();
    process.waitForFinished(1000);
    buffer.clear();
    pending.clear();
}
void TerminalSession::send(const QByteArray &j) {
    if (process.state() == QProcess::Running)
        process.write(j + "\n");
}
void TerminalSession::input(const QString &s) {
    send(QJsonDocument(QJsonObject{{"input", s}}).toJson(QJsonDocument::Compact));
}
void TerminalSession::resize(int c, int r) {
    send(QJsonDocument(QJsonObject{{"cols", qBound(10, c, 500)}, {"rows", qBound(2, r, 200)}})
             .toJson(QJsonDocument::Compact));
}
