#include "PythonEnvironment.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
PythonEnvironment::PythonEnvironment(QObject *p) : QObject(p) {
    connect(&creator, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus) {
                busy = false;
                emit changed();
                if (code == 0) {
                    const auto starter = QDir(creatingProject).filePath("main.py");
                    if (!QFileInfo::exists(starter)) {
                        QSaveFile file(starter);
                        const QByteArray source = u8"# Welcome to EduCode\nname = input(\"Как тебя зовут? \" "
                                                  u8")\nprint(f\"Привет, {name}!\")\n";
                        if (!file.open(QIODevice::WriteOnly) || file.write(source) != source.size() ||
                            !file.commit()) {
                            emit error("Не удалось записать стартовый файл: " + file.errorString());
                            return;
                        }
                    }
                    emit created(creatingProject);
                } else
                    emit error(QString::fromUtf8(creator.readAllStandardError()));
            });
    connect(&creator, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            busy = false;
            emit changed();
            emit error(creator.errorString());
        }
    });
}
QString PythonEnvironment::interpreter(const QString &p) {
#ifdef Q_OS_WIN
    return QDir(p).filePath(".venv/Scripts/python.exe");
#else
    return QDir(p).filePath(".venv/bin/python");
#endif
}
void PythonEnvironment::discover() {
    for (const QString &name : {QStringLiteral("python3"), QStringLiteral("python")}) {
        QString p = QStandardPaths::findExecutable(name);
        if (!p.isEmpty() && !p.contains("WindowsApps"))
            candidates << p;
    }
#ifdef Q_OS_WIN
    QDir dir(QDir::homePath() + "/AppData/Local/Programs/Python");
    for (const auto &n : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        candidates << dir.filePath(n + "/python.exe");
    for (const auto &key : {QStringLiteral("HKEY_CURRENT_USER\\Software\\Python\\PythonCore"),
                            QStringLiteral("HKEY_LOCAL_MACHINE\\Software\\Python\\PythonCore")}) {
        QSettings s(key, QSettings::NativeFormat);
        for (const auto &v : s.childGroups()) {
            s.beginGroup(v + "/InstallPath");
            QString p = s.value("ExecutablePath").toString();
            if (p.isEmpty())
                p = QDir(s.value(".").toString()).filePath("python.exe");
            candidates << p;
            s.endGroup();
        }
    }
    auto launcher = new QProcess(this);
    connect(launcher, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, launcher] {
        QString out = QString::fromUtf8(launcher->readAllStandardOutput());
        for (const auto &line : out.split('\n')) {
            int pos = line.indexOf(":\\");
            if (pos > 0)
                candidates << line.mid(pos - 1).trimmed();
        }
        launcher->deleteLater();
        next();
    });
    connect(launcher, &QProcess::errorOccurred, this, [this, launcher](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            launcher->deleteLater();
            next();
        }
    });
    launcher->start("py", {"-0p"});
#else
    for (const auto &n : QDir("/usr/bin").entryList({"python3.*"}, QDir::Files))
        candidates << "/usr/bin/" + n;
    next();
#endif
}
void PythonEnvironment::probe(const QString &path) {
    candidates << path;
    next();
}
void PythonEnvironment::next() {
    if (candidates.isEmpty())
        return;
    QString path = candidates.takeFirst();
    if (!QFileInfo::exists(path) || seen.contains(path)) {
        next();
        return;
    }
    seen.insert(path);
    auto p = new QProcess(this);
    auto timeout = new QTimer(p);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, p, &QProcess::kill);
    timeout->start(5000);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, p, path](int code, QProcess::ExitStatus) {
                if (code == 0) {
                    auto obj = QJsonDocument::fromJson(p->readAllStandardOutput()).object();
                    QString exe = obj["path"].toString();
                    bool duplicate = false;
                    for (const auto &v : versions)
                        if (v.toMap()["path"].toString() == exe)
                            duplicate = true;
                    if (!duplicate && !exe.isEmpty()) {
                        versions.append(QVariantMap{{"path", exe},
                                                    {"label", "Python " + obj["version"].toString()},
                                                    {"version", obj["version"].toString()}});
                        emit changed();
                    }
                }
                p->deleteLater();
                next();
            });
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            p->deleteLater();
            next();
        }
    });
    p->start(
        path,
        {"-c",
         "import sys,json; "
         "print(json.dumps({'path':sys.executable,'version':'.'.join(map(str,sys.version_info[:3]))}))"});
}
void PythonEnvironment::create(const QString &p, const QString &python) {
    if (busy)
        return;
    creatingProject = p;
    if (!QDir().mkpath(p)) {
        emit error("Не удалось создать папку проекта.");
        return;
    }
    busy = true;
    emit changed();
    creator.start(python, {"-m", "venv", QDir(p).filePath(".venv")});
}
