#include "SearchService.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>
namespace {
QVariantList match(const QVariantList &items, const QString &query) {
    if (query.trimmed().isEmpty())
        return items.mid(0, 60);
    QVariantList out;
    QList<QPair<int, QVariant>> hits;
    const auto words = query.toLower().split(' ', Qt::SkipEmptyParts);
    for (const auto &item : items) {
        const auto m = item.toMap();
        const auto haystack = (m.value("title").toString() + " " + m.value("detail").toString()).toLower();
        bool ok = true;
        for (const auto &word : words)
            if (!haystack.contains(word)) {
                ok = false;
                break;
            }
        if (ok) {
            const auto title = m.value("title").toString().toLower();
            const auto needle = query.toLower();
            int rank = title == needle ? 0 : title.startsWith(needle) ? 1 : title.contains(needle) ? 2 : 3;
            hits.append({rank, item});
        }
    }
    std::stable_sort(hits.begin(), hits.end(),
                     [](const auto &a, const auto &b) { return a.first < b.first; });
    for (int i = 0; i < qMin(60, hits.size()); ++i)
        out << hits[i].second;
    return out;
}
} // namespace
SearchService::SearchService(QObject *parent) : QObject(parent) {
    debounce.setSingleShot(true);
    debounce.setInterval(80);
    libraryTimer.setSingleShot(true);
    libraryTimer.setInterval(500);
    connect(&libraryWatcher, &QFileSystemWatcher::directoryChanged, this, [this] { libraryTimer.start(); });
    connect(&libraryTimer, &QTimer::timeout, this, [this] {
        if (root.isEmpty())
            return;
        if (modules.state() == QProcess::NotRunning)
            loadLibraries();
        else
            libraryTimer.start();
    });
    refreshTimer.setSingleShot(true);
    refreshTimer.setInterval(150);
    connect(&debounce, &QTimer::timeout, this, [this] {
        contentResults.clear();
        publish();
        findContent();
    });
    connect(&contentScanner, &QFutureWatcher<QVariantList>::finished, this, [this] {
        if (contentQuery == expression && contentRoot == root) {
            contentResults = contentScanner.result();
            publish();
        }
        if (contentPending) {
            contentPending = false;
            findContent();
        }
    });
    connect(&refreshTimer, &QTimer::timeout, this, &SearchService::refresh);
    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, [this] { refreshTimer.start(); });
    connect(&scanner, &QFutureWatcher<QVariantList>::finished, this, [this] {
        if (scannedRoot != root) {
            rescan = false;
            refresh();
            return;
        }
        projectFiles.clear();
        runTargets.clear();
        QStringList dirs{root};
        for (const auto &v : scanner.result()) {
            auto m = v.toMap();
            const auto path = m["path"].toString();
            if (m["directory"].toBool()) {
                dirs << path;
                continue;
            }
            projectFiles << v;
            if (path.endsWith(".py", Qt::CaseInsensitive))
                runTargets << QVariantMap{{"path", path}, {"name", m["detail"]}};
            dirs << QFileInfo(path).absolutePath();
        }
        dirs.removeDuplicates();
        if (!watcher.directories().isEmpty())
            watcher.removePaths(watcher.directories());
        watcher.addPaths(dirs);
        emit indexChanged();
        publish();
        findContent();
        if (rescan) {
            rescan = false;
            refresh();
        }
    });
    connect(&modules, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus) {
                if (code == 0) {
                    const auto index = QJsonDocument::fromJson(modules.readAllStandardOutput()).object();
                    libraries = index["items"].toArray().toVariantList();
                    if (!libraryWatcher.directories().isEmpty())
                        libraryWatcher.removePaths(libraryWatcher.directories());
                    QStringList directories;
                    for (const auto &value : index["watch"].toArray())
                        directories << value.toString();
                    directories.removeDuplicates();
                    libraryWatcher.addPaths(directories);
                }
                publish();
                findContent();
            });
    registerProvider("project", [this](const QString &q) { return match(projectFiles, q); });
    registerProvider("libraries",
                     [this](const QString &q) { return q.isEmpty() ? QVariantList{} : match(libraries, q); });
    registerProvider("calculator", [](const QString &q) {
        QVariantList out;
        QString expr = q.trimmed();
        if (expr.startsWith('='))
            expr = expr.mid(1).trimmed();
        if (expr.size() > 200 || !expr.contains(QRegularExpression("[+*/%()-]")) ||
            !QRegularExpression("^[0-9eE. +*/%()-]+$").match(expr).hasMatch())
            return out;
        QJSEngine engine;
        auto value = engine.evaluate("(" + expr + ")");
        if (value.isNumber() && std::isfinite(value.toNumber())) {
            QString answer = QString::number(value.toNumber(), 'g', 15);
            out << QVariantMap{{"title", answer},
                               {"detail", expr + " · Enter — скопировать результат"},
                               {"value", answer},
                               {"group", "Калькулятор"}};
        }
        return out;
    });
}
SearchService::~SearchService() {
    modules.disconnect(this);
    if (modules.state() != QProcess::NotRunning) {
        modules.kill();
        modules.waitForFinished(1000);
    }
}
void SearchService::registerProvider(const QString &id, Provider provider) {
    providers.append({id, std::move(provider)});
}
void SearchService::setProject(const QString &path, const QString &python) {
    root = path;
    interpreter = python;
    if (!libraryWatcher.directories().isEmpty())
        libraryWatcher.removePaths(libraryWatcher.directories());
    openBuffers.clear();
    contentResults.clear();
    libraries.clear();
    projectFiles.clear();
    runTargets.clear();
    if (modules.state() != QProcess::NotRunning) {
        modules.kill();
        modules.waitForFinished(1000);
    }
    refresh();
    if (!root.isEmpty()) {
        loadLibraries();
    } else
        libraryTimer.stop();
}
void SearchService::loadLibraries() {
    const auto base = QCoreApplication::applicationDirPath();
    modules.start(interpreter, {"-I", base + "/tools/index-python.py",
                                base + "/node_modules/pyright/dist/typeshed-fallback/stdlib"});
}
void SearchService::refresh() {
    if (scanner.isRunning()) {
        rescan = true;
        return;
    }
    const auto path = root;
    scannedRoot = root;
    scanner.setFuture(QtConcurrent::run([path] {
        QVariantList out;
        if (path.isEmpty())
            return out;
        std::function<void(const QString &, int)> walk = [&](const QString &dir, int depth) {
            if (depth > 48 || out.size() >= 20000)
                return;
            for (const auto &f :
                 QDir(dir).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
                if (f.isSymLink())
                    continue;
                if (f.isDir()) {
                    if (!QStringList{".venv", ".git", "__pycache__", ".educode", "node_modules"}.contains(
                            f.fileName())) {
                        out << QVariantMap{{"path", f.absoluteFilePath()}, {"directory", true}};
                        walk(f.absoluteFilePath(), depth + 1);
                    }
                } else
                    out << QVariantMap{{"title", f.fileName()},
                                       {"detail", QDir(path).relativeFilePath(f.absoluteFilePath())},
                                       {"path", f.absoluteFilePath()},
                                       {"group", "Файлы проекта"}};
                if (out.size() >= 20000)
                    break;
            }
        };
        walk(path, 0);
        return out;
    }));
}
void SearchService::query(const QString &q) {
    expression = q;
    debounce.start();
}
void SearchService::publish() {
    results.clear();
    for (const auto &provider : providers) {
        results.append(provider.second(expression));
        if (provider.first == "project" || provider.first == "libraries")
            for (const auto &result : contentResults)
                if (result.toMap()["provider"].toString() == provider.first)
                    results.append(result);
    }
    emit changed();
}

void SearchService::updateDocument(const QString &path, const QString &text) {
    openBuffers[QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath())] = text;
    if (!expression.isEmpty())
        debounce.start();
}
void SearchService::forgetDocument(const QString &path) {
    openBuffers.remove(QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
    if (!expression.isEmpty())
        debounce.start();
}
void SearchService::findContent() {
    if (contentScanner.isRunning()) {
        contentPending = true;
        return;
    }
    const auto needle = expression.trimmed();
    if (needle.size() < 2 || needle.size() > 200)
        return;
    contentQuery = expression;
    contentRoot = root;
    const auto projectIndex = projectFiles, libraryIndex = libraries;
    const auto buffers = openBuffers;
    contentScanner.setFuture(QtConcurrent::run([needle, projectIndex, libraryIndex, buffers] {
        QVariantList out;
        qint64 bytes = 0;
        for (int provider = 0; provider < 2; ++provider) {
            int count = 0;
            const auto &index = provider == 0 ? projectIndex : libraryIndex;
            for (const auto &entry : index) {
                auto item = entry.toMap();
                const auto path = item["path"].toString();
                if (path.isEmpty())
                    continue;
                QString text;
                if (buffers.contains(path))
                    text = buffers[path];
                else {
                    QFile file(path);
                    if (file.size() > 2 * 1024 * 1024 || !file.open(QIODevice::ReadOnly))
                        continue;
                    const auto data = file.readAll();
                    if (data.contains(char(0)))
                        continue;
                    bytes += data.size();
                    if (bytes > 64 * 1024 * 1024)
                        break;
                    text = QString::fromUtf8(data);
                }
                int offset = 0, hit = 0;
                while ((offset = text.indexOf(needle, offset, Qt::CaseInsensitive)) >= 0 && hit < 3 &&
                       count < 60) {
                    int line = 1;
                    for (int i = 0; i < offset; ++i)
                        if (text[i] == '\n')
                            ++line;
                    const int start = offset > 0 ? text.lastIndexOf('\n', offset - 1) + 1 : 0;
                    int end = text.indexOf('\n', offset);
                    if (end < 0)
                        end = text.size();
                    auto result =
                        QVariantMap{{"title", QFileInfo(path).fileName() + ":" + QString::number(line)},
                                    {"detail", text.mid(start, end - start).trimmed().left(180)},
                                    {"path", path},
                                    {"line", line},
                                    {"column", offset - start + 1},
                                    {"provider", provider == 0 ? "project" : "libraries"},
                                    {"group", provider == 0 ? "Код проекта" : "Код библиотек"}};
                    out << result;
                    ++hit;
                    ++count;
                    offset += needle.size();
                }
                if (count >= 60)
                    break;
            }
        }
        return out;
    }));
}
