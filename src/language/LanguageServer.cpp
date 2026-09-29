#include "LanguageServer.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>
LanguageServer::LanguageServer(QObject *p) : QObject(p) {
    connect(&process, &QProcess::readyReadStandardOutput, this, &LanguageServer::read);
    connect(&process, &QProcess::started, this, [this] {
        send(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", -1},
            {"method", "initialize"},
            {"params",
             QJsonObject{
                 {"processId", QJsonValue::Null},
                 {"rootUri", QUrl::fromLocalFile(root).toString()},
                 {"capabilities",
                  QJsonObject{
                      {"textDocument",
                       QJsonObject{
                           {"completion", QJsonObject{{"completionItem",
                                                       QJsonObject{{"snippetSupport", true},
                                                                   {"documentationFormat",
                                                                    QJsonArray{"markdown", "plaintext"}}}}}},
                           {"hover", QJsonObject{{"contentFormat", QJsonArray{"markdown", "plaintext"}}}},
                           {"signatureHelp",
                            QJsonObject{
                                {"signatureInformation",
                                 QJsonObject{{"documentationFormat", QJsonArray{"markdown", "plaintext"}}}}}},
                           {"publishDiagnostics", QJsonObject{{"versionSupport", true}}}}},
                      {"workspace", QJsonObject{{"configuration", true}}}}},
                 {"initializationOptions", QJsonObject{}}}}});
    });
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            emit error("Pyright: " + process.errorString());
    });
    connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus) {
                bool was = ready;
                ready = false;
                if (was && code != 0)
                    emit error("Сервер Python завершился: " +
                               QString::fromUtf8(process.readAllStandardError()).left(1000));
            });
}
LanguageServer::~LanguageServer() {
    stop();
}
void LanguageServer::stop() {
    ready = false;
    process.kill();
    process.waitForFinished(1000);
    buffer.clear();
}
void LanguageServer::start(const QString &n, const QString &s, const QString &r, const QString &py) {
    stop();
    root = r;
    python = py;
    process.setWorkingDirectory(root);
    process.start(n, {s, "--stdio"});
}
void LanguageServer::send(const QJsonObject &obj) {
    QByteArray bytes = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    process.write("Content-Length: " + QByteArray::number(bytes.size()) + "\r\n\r\n" + bytes);
}
void LanguageServer::relay(const QString &msg) {
    auto d = QJsonDocument::fromJson(msg.toUtf8());
    if (d.isObject() && ready)
        send(d.object());
}
void LanguageServer::read() {
    buffer += process.readAllStandardOutput();
    for (;;) {
        int sep = buffer.indexOf("\r\n\r\n");
        if (sep < 0)
            return;
        int size = -1;
        for (const auto &line : buffer.left(sep).split('\n'))
            if (line.toLower().startsWith("content-length:"))
                size = line.mid(15).trimmed().toInt();
        if (size < 0 || size > 32 * 1024 * 1024) {
            buffer.clear();
            emit error("Некорректный пакет LSP.");
            return;
        }
        if (buffer.size() < sep + 4 + size)
            return;
        auto obj = QJsonDocument::fromJson(buffer.mid(sep + 4, size)).object();
        buffer.remove(0, sep + 4 + size);
        handle(obj);
    }
}
void LanguageServer::handle(const QJsonObject &obj) {
    if (obj["id"].toInt() == -1 && !obj.contains("method")) {
        ready = true;
        send({{"jsonrpc", "2.0"}, {"method", "initialized"}, {"params", QJsonObject{}}});
        QJsonObject analysis{{"diagnosticMode", "openFilesOnly"},
                             {"typeCheckingMode", "basic"},
                             {"autoSearchPaths", true},
                             {"useLibraryCodeForTypes", true},
                             {"extraPaths", QJsonArray::fromStringList(QStringList{root} + extraPaths)}};
        QJsonObject settings{{"python", QJsonObject{{"pythonPath", python}, {"analysis", analysis}}}};
        send({{"jsonrpc", "2.0"},
              {"method", "workspace/didChangeConfiguration"},
              {"params", QJsonObject{{"settings", settings}}}});
        emit initialized();
        return;
    }
    QString method = obj["method"].toString();
    if (obj.contains("id") && !method.isEmpty()) {
        QJsonValue result = QJsonValue::Null;
        if (method == "workspace/configuration") {
            QJsonArray arr;
            for (const auto &item : obj["params"].toObject()["items"].toArray()) {
                QString section = item.toObject()["section"].toString();
                if (section == "python")
                    arr.append(QJsonObject{{"pythonPath", python}});
                else if (section == "python.analysis")
                    arr.append(QJsonObject{{"diagnosticMode", "openFilesOnly"},
                                           {"typeCheckingMode", "basic"},
                                           {"autoSearchPaths", true},
                                           {"useLibraryCodeForTypes", true},
                                           {"extraPaths", QJsonArray::fromStringList(QStringList{root} + extraPaths)}});
                else
                    arr.append(QJsonObject{});
            }
            result = arr;
        } else if (method == "workspace/workspaceFolders")
            result = QJsonArray{
                QJsonObject{{"uri", QUrl::fromLocalFile(root).toString()}, {"name", root.section('/', -1)}}};
        send({{"jsonrpc", "2.0"}, {"id", obj["id"]}, {"result", result}});
        return;
    }
    emit message(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void LanguageServer::configure(const QStringList &paths) {
    extraPaths = paths;
    if (ready)
        send({{"jsonrpc", "2.0"},
              {"method", "workspace/didChangeConfiguration"},
              {"params",
               QJsonObject{
                   {"settings",
                    QJsonObject{{"python", QJsonObject{{"pythonPath", python},
                                                       {"analysis",
                                                        QJsonObject{{"diagnosticMode", "openFilesOnly"},
                                                                    {"typeCheckingMode", "basic"},
                                                                    {"autoSearchPaths", true},
                                                                    {"useLibraryCodeForTypes", true},
                                                                    {"extraPaths", QJsonArray::fromStringList(
                                                                                       QStringList{root} + extraPaths)}}}}}}}}}});
}

void LanguageServer::fileChanged(const QString &path) {
    if (ready) send({{"jsonrpc", "2.0"}, {"method", "workspace/didChangeWatchedFiles"},
        {"params", QJsonObject{{"changes", QJsonArray{QJsonObject{{"uri", QUrl::fromLocalFile(path).toString()}, {"type", 2}}}}}}});
}
