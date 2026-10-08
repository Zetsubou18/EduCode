#include "AppController.h"
#include "Log.h"
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>
#include <QVersionNumber>
#include <QProcess>
#include <QProcessEnvironment>
AppController::AppController(QObject *p)
    : QObject(p), config(this), control(this), tray(this), search(this), docs(this), python(this), fs(this),
      runner(this), language(this), terminal(this), packageManager(this), assistant(this) {
    connect(&teacherSession, &TeacherSession::changed, this, [this] { runner.demoMode = teacherSession.property("role").toString() == "teacher"; });
    connect(&teacherSession, &TeacherSession::connectionLost, this, [this](const QString &reason) {
        notify(QStringLiteral("Teacher Mode: соединение потеряно"),
               reason + QStringLiteral(". Нажмите «Переподключиться» в панели Teacher Mode."));
    });
    teacherSession.displayName = [this] { return config.values["general.displayName"].toString(); };
    teacherSession.document = [this] { return QJsonObject{{"path", docs.active}, {"text", docs.entries.value(docs.active).text}}; };
    teacherSession.applyEdit = [this](const QString &path, const QString &text) {
        if (path != docs.active || !docs.entries.contains(path)) return;
        editDocument(path, text);
        emit sourceActivated(path, text, 0, 0);
    };
    qputenv("EDUCODE_CONTROL", control.endpointPath.toUtf8());
    qputenv("EDUCODE_PYTHON_TOOLS", runtime("tools").toUtf8());
    QTimer::singleShot(0, this, &AppController::refreshPlugins);
    tray.setIcon(QIcon(runtime("assets/app_logo.png")));
    tray.setToolTip("EduCode");
    if (QSystemTrayIcon::isSystemTrayAvailable())
        tray.show();
    connect(&tray, &QSystemTrayIcon::messageClicked, this, [this] { emit sidebarRequested(0); });
    connect(&config, &Configuration::changed, this, [this] {
        language.configure(config.values["python.extraPaths"].toStringList() + QStringList{runtime("tools")});
        assistant.configure(config.values, runtime("tools/ai-agent.py"), control.endpointPath, pythonPath());
        emit configurationChanged();
        Log::write("CONFIG", "Configuration updated");
    });
    connect(&config, &Configuration::rejected, this, &AppController::error);
    connect(this, &AppController::error, this, [](const QString &message) { Log::write("ERROR", message); });
    connect(&packageManager, &PackageManager::error, this, &AppController::error);
    connect(&packageManager, &PackageManager::packagesChanged, this, [this] {
        search.refresh();
        search.refreshLibraries();
        if (!project.isEmpty() && QFileInfo::exists(pythonPath())) {
            beginTask("pyright", QStringLiteral("Python-анализатор"), QStringLiteral("Перечитываю установленные библиотеки…"), 20);
            language.start(node(), runtime("node_modules/pyright/dist/pyright-langserver.js"), project, pythonPath());
        }
    });
    connect(&packageManager, &PackageManager::changed, this, [this] {
        if (packageManager.busy())
            beginTask("packages", QStringLiteral("Менеджер библиотек"), packageManager.status(), 35);
        else if (tasks.contains("packages"))
            finishTask("packages", packageManager.status());
    });
    connect(&assistant, &AiAssistant::error, this, &AppController::error);
    connect(&assistant, &AiAssistant::requestInstall, this,
            [] { QDesktopServices::openUrl(QUrl("https://ollama.com/download")); });
    control.dispatch = [this](const QVariantMap &request) {
        const auto method = request["method"].toString();
        const auto args = request["params"].toMap();
        if (qEnvironmentVariableIsSet("EDUCODE_TEST_MODE") && method.startsWith("teacher.")) {
            auto action = method.mid(8);
            if (action == "ui") emit teacherSession.uiCommand(args["name"].toString());
            if (action == "create") teacherSession.create();
            if (action == "join") teacherSession.join(args["ip"].toString(), args["port"].toInt());
            if (action == "stop") teacherSession.stop();
            if (action == "watch") teacherSession.watch(args["id"].toString());
            if (action == "kick") teacherSession.kick(args["id"].toString());
            if (action == "edit") teacherSession.editRemote(args["text"].toString());
            if (action == "localEdit") { editDocument(docs.active, args["text"].toString()); emit sourceActivated(docs.active, args["text"].toString(), 0, 0); }
            QVariantMap result;
            for (const auto &key : QStringList{"role", "port", "students", "remotePath", "remoteText", "frame", "selected", "status"}) result[key] = teacherSession.property(key.toUtf8());
            return QVariantMap{{"ok", true}, {"result", result}};
        }
        if (qEnvironmentVariableIsSet("EDUCODE_TEST_MODE") && method == "test.ui") {
            emit editorCommand(args["command"].toString());
            return QVariantMap{{"ok", true}};
        }
        if (method == "notify")
            return QVariantMap{{"ok", notify(args["title"].toString(), args["message"].toString())}};
        if (method == "config.get")
            return QVariantMap{{"ok", true}, {"result", [&] { auto v = config.values; v.remove("ai.geminiApiKey"); return v; }()}};
        if (method == "config.patch") {
            QString message;
            const bool ok = config.patch(args, &message);
            return QVariantMap{{"ok", ok}, {"error", message}, {"result", [&] { auto v = config.values; v.remove("ai.geminiApiKey"); return v; }()}};
        }
        if (method == "command" && Commands::defaultBindings().contains(args["name"].toString())) {
            command(args["name"].toString());
            return QVariantMap{{"ok", true}};
        }
        auto safePath = [this](const QString &input, bool existing) {
            QString path =
                QDir::cleanPath(QFileInfo(input).isAbsolute() ? input : QDir(project).filePath(input));
            QString root = QFileInfo(project).canonicalFilePath();
            if (existing) {
                path = QFileInfo(path).canonicalFilePath();
            } else {
                QString parent = QFileInfo(path).absolutePath();
                while (!QFileInfo::exists(parent) && QFileInfo(parent).absolutePath() != parent)
                    parent = QFileInfo(parent).absolutePath();
                const QString canonicalParent = QFileInfo(parent).canonicalFilePath();
                if (canonicalParent.isEmpty())
                    return QString();
                path = QDir(canonicalParent)
                           .filePath(QDir(parent).relativeFilePath(QFileInfo(path).absoluteFilePath()));
            }
#ifdef Q_OS_WIN
            constexpr auto sensitivity = Qt::CaseInsensitive;
#else
            constexpr auto sensitivity = Qt::CaseSensitive;
#endif
            bool inside = !root.isEmpty() && (path.compare(root, sensitivity) == 0 ||
                                              path.startsWith(root + "/", sensitivity));
            if (!inside || (existing && !QFileInfo::exists(path)))
                return QString();
            return path;
        };
        if (method == "ai.context") {
            QFile log(Log::path());
            QString logTail;
            if (log.open(QIODevice::ReadOnly)) {
                if (log.size() > 20000)
                    log.seek(log.size() - 20000);
                logTail = QString::fromUtf8(log.readAll());
            }
            return QVariantMap{
                {"ok", true},
                {"result", QVariantMap{{"project", project},
                                       {"activeFile", docs.active},
                                       {"activeCode", docs.entries.value(docs.active).text.left(50000)},
                                       {"problems", currentProblems},
                                       {"console", consoleBuffer.right(30000)},
                                       {"terminal", terminalBuffer.right(30000)},
                                       {"packages", packageManager.items()},
                                        {"settings", [&] { auto v = config.values; v.remove("ai.geminiApiKey"); return v; }()},
                                       {"log", logTail},
                                       {"time", QDateTime::currentDateTime().toString(Qt::ISODate)},
                                       {"os", QSysInfo::prettyProductName()},
                                       {"architecture", QSysInfo::currentCpuArchitecture()}}}};
        }
        if (method == "project.list") {
            QVariantList files;
            QDirIterator it(project, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext() && files.size() < 5000) {
                auto p = it.next();
                if (p.contains("/.venv/") || p.contains("\\.venv\\") || p.contains("/.git/") ||
                    p.contains("\\.git\\"))
                    continue;
                files << QDir(project).relativeFilePath(p);
            }
            return QVariantMap{{"ok", true}, {"result", files}};
        }
        if (method == "project.read") {
            const auto p = safePath(args["path"].toString(), true);
            QFile f(p);
            if (p.isEmpty() || !f.open(QIODevice::ReadOnly) || f.size() > 2 * 1024 * 1024)
                return QVariantMap{{"ok", false}, {"error", "Файл недоступен или слишком велик."}};
            return QVariantMap{{"ok", true}, {"result", QString::fromUtf8(f.readAll())}};
        }
        if (method == "project.write") {
            const auto p = safePath(args["path"].toString(), false);
            if (p.isEmpty() || args["content"].toString().size() > 2 * 1024 * 1024)
                return QVariantMap{{"ok", false}, {"error", "Путь вне проекта или файл слишком велик."}};
            QDir().mkpath(QFileInfo(p).absolutePath());
            QSaveFile f(p);
            auto bytes = args["content"].toString().toUtf8();
            if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit())
                return QVariantMap{{"ok", false}, {"error", "Не удалось записать файл."}};
            fs.refresh();
            search.refresh();
            if (docs.entries.contains(p)) {
                docs.entries[p].text = args["content"].toString();
                docs.entries[p].saved = docs.entries[p].text;
                activate();
            }
            language.fileChanged(p);
            Log::write("AI", "Wrote project file " + p);
            return QVariantMap{{"ok", true}, {"result", p}};
        }
        if (method == "project.delete") {
            const auto p = safePath(args["path"].toString(), true);
            QString error;
            if (p.isEmpty() || QFileInfo(p).isDir() || !fs.remove(p, &error))
                return QVariantMap{{"ok", false}, {"error", error.isEmpty() ? "Удаление запрещено." : error}};
            return QVariantMap{{"ok", true}};
        }
        if (method == "console.input") {
            consoleInput(args["text"].toString());
            return QVariantMap{{"ok", true}};
        }
        if (method == "terminal.input") {
            startTerminal();
            const auto text = args["text"].toString();
            QTimer::singleShot(400, this, [this, text] { terminalInput(text); });
            return QVariantMap{{"ok", true}};
        }
        if (method == "package.action") {
            const auto action = args["action"].toString(), name = args["name"].toString();
            if (action == "install")
                packageManager.install(name, QString());
            else if (action == "update")
                packageManager.update(name);
            else if (action == "remove")
                packageManager.remove(name);
            else
                return QVariantMap{{"ok", false}, {"error", "Неизвестная операция пакета."}};
            return QVariantMap{{"ok", true}, {"result", "Операция запущена"}};
        }
        if (method == "browser.open") {
            const QUrl url(args["url"].toString());
            if (!url.isValid() || !QStringList{"http", "https"}.contains(url.scheme()))
                return QVariantMap{{"ok", false}, {"error", "Разрешены только HTTP(S)-адреса."}};
            emit browserRequested(url.toString());
            return QVariantMap{{"ok", true}};
        }
        if (method == "memory.update") {
            const auto key = args["key"].toString().trimmed(), kind = args["kind"].toString();
            if (key.isEmpty() || (kind != "user" && kind != "facts"))
                return QVariantMap{{"ok", false}, {"error", "Некорректная память."}};
            auto memory = config.values["ai.factMemory"].toMap();
            if (args["value"].toString().isEmpty())
                memory.remove(key);
            else
                memory[key] = args["value"].toString().left(2000);
            QString error;
            bool ok = config.set("ai.factMemory", memory, &error);
            return QVariantMap{{"ok", ok}, {"error", error}, {"result", memory}};
        }
        return QVariantMap{{"ok", false}, {"error", "Unknown method"}};
    };
    connect(&search, &SearchService::changed, this, &AppController::searchChanged);
    connect(&search, &SearchService::scanStarted, this, [this] {
        if (!project.isEmpty()) beginTask("project-index", QStringLiteral("Индексирование проекта"), QStringLiteral("Сканирую файлы…"), 20);
    });
    connect(&search, &SearchService::scanFinished, this, [this](int files) {
        finishTask("project-index", QStringLiteral("Файлов: %1").arg(files));
    });
    connect(&search, &SearchService::libraryIndexStarted, this, [this] {
        if (!project.isEmpty()) beginTask("library-index", QStringLiteral("Индексирование Python"), QStringLiteral("Анализирую библиотеки…"), 15);
    });
    connect(&search, &SearchService::libraryIndexFinished, this, [this](int entries, bool ok) {
        finishTask("library-index", ok ? QStringLiteral("Символов: %1").arg(entries) : QStringLiteral("Ошибка индексирования"));
    });
    connect(this, &AppController::documentClosed, &search, &SearchService::forgetDocument);
    connect(&search, &SearchService::indexChanged, this, [this] {
        if (!target.isEmpty() && !QFileInfo::exists(target)) {
            target.clear();
            emit stateChanged();
        }
        emit filesChanged();
    });
    connect(&fs, &FileSystem::changed, &search, &SearchService::refresh);
    connect(&docs, &Documents::changed, this, &AppController::documentsChanged);
    connect(&fs, &FileSystem::changed, this, &AppController::filesChanged);
    connect(&fs, &FileSystem::pathMoved, this, &AppController::relocateDocuments);
    connect(&fs, &FileSystem::transferFinished, this, [this](const QString &message) {
        if (!message.isEmpty())
            emit error(message);
    });
    connect(&python, &PythonEnvironment::changed, this, &AppController::stateChanged);
    connect(&python, &PythonEnvironment::created, this, [this](const QString &p) { finishTask("python-env", QStringLiteral("Среда готова")); enterProject(p); });
    connect(&python, &PythonEnvironment::error, this, [this](const QString &text) { finishTask("python-env", QStringLiteral("Ошибка")); emit error(text); });
    connect(&runner, &ProcessRunner::output, this, [this](const QString &text) {
        consoleBuffer += text;
        if (consoleBuffer.size() > 100000)
            consoleBuffer = consoleBuffer.right(100000);
        if (text.contains("Traceback") || text.contains(QRegularExpression("[A-Za-z_]*(Error|Exception):")))
            hasConsoleError = true;
        emit consoleOutput(text);
        emit stateChanged();
    });
    connect(&runner, &ProcessRunner::runningChanged, this, [this] {
        Log::write("RUN", runner.running() ? "Process starting/running" : "Process finished");
        emit stateChanged();
    });
    connect(&terminal, &TerminalSession::output, this, [this](const QString &text) {
        terminalBuffer += text;
        if (terminalBuffer.size() > 100000)
            terminalBuffer = terminalBuffer.right(100000);
        emit terminalOutput(text);
    });
    connect(&terminal, &TerminalSession::error, this, &AppController::error);
    connect(&language, &LanguageServer::message, this, &AppController::lspMessage);
    connect(&language, &LanguageServer::initialized, this, [this] {
        currentStatus = "Готово";
        finishTask("pyright", QStringLiteral("Анализ кода готов"));
        emit stateChanged();
        emit languageReady();
    });
    connect(&language, &LanguageServer::error, this, [this](const QString &text) {
        currentStatus = "Python: сервер недоступен";
        finishTask("pyright", QStringLiteral("Сервер недоступен"));
        emit stateChanged();
        emit error(text);
    });
    language.configure(config.values["python.extraPaths"].toStringList() + QStringList{runtime("tools")});
    assistant.configure(config.values, runtime("tools/ai-agent.py"), control.endpointPath, QString());
    auto snapshotTimer = new QTimer(this);
    snapshotTimer->setInterval(5 * 60 * 1000);
    connect(snapshotTimer, &QTimer::timeout, this, &AppController::writeAutoSnapshot);
    snapshotTimer->start();
    QTimer::singleShot(3000, this, &AppController::writeAutoSnapshot);
    QTimer::singleShot(0, &python, &PythonEnvironment::discover);
    if (!qEnvironmentVariableIsSet("EDUCODE_TEST_MODE"))
        QTimer::singleShot(1800, this, &AppController::checkForUpdates);
}

void AppController::checkForUpdates() {
    if (!config.values.value("updates.enabled", true).toBool())
        return;
    QNetworkRequest request{QUrl("https://api.github.com/repos/Zetsubou18/EduCode/releases/latest")};
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "EduCode/" + QCoreApplication::applicationVersion().toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto reply = updateNetwork.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const auto data = reply->readAll();
        const auto networkError = reply->error();
        const auto networkMessage = reply->errorString();
        reply->deleteLater();
        if (networkError != QNetworkReply::NoError) {
            Log::write("UPDATE", "Update check skipped: " + networkMessage);
            return;
        }
        const auto object = QJsonDocument::fromJson(data).object();
        auto tag = object.value("tag_name").toString().trimmed();
        if (tag.startsWith('v', Qt::CaseInsensitive))
            tag.remove(0, 1);
        const auto available = QVersionNumber::fromString(tag);
        const auto current = QVersionNumber::fromString(QCoreApplication::applicationVersion());
        if (!available.isNull() && QVersionNumber::compare(available, current) > 0)
            notify(QStringLiteral("Доступно обновление EduCode ") + tag,
                   QStringLiteral("Новая версия опубликована на GitHub. Откройте репозиторий EduCode, чтобы скачать обновление."));
        else
            Log::write("UPDATE", "No newer GitHub release found");
    });
}
QString AppController::runtime(const QString &p) const {
    if (p == "tools/ai-agent.py" && qEnvironmentVariableIsSet("EDUCODE_TEST_MODE") &&
        !qEnvironmentVariable("EDUCODE_TEST_AI_HELPER").isEmpty())
        return qEnvironmentVariable("EDUCODE_TEST_AI_HELPER");
    return QDir(QCoreApplication::applicationDirPath()).filePath(p);
}
QString AppController::node() const {
    QString bundled = runtime("node.exe");
    return QFileInfo::exists(bundled) ? bundled : QStandardPaths::findExecutable("node");
}
QString AppController::projectName() const {
    return QFileInfo(project).fileName();
}
QString AppController::projectsDirectory() const {
    return config.values["general.projectsDirectory"].toString();
}
QVariantList AppController::recentProjects() const {
    QVariantList out;
    for (const auto &p : prefs.value("recent").toStringList())
        out.append(
            QVariantMap{{"path", p}, {"name", QFileInfo(p).fileName()}, {"exists", QFileInfo(p).isDir()}});
    return out;
}
void AppController::continueProject() {
    auto recent = prefs.value("recent").toStringList();
    if (!recent.isEmpty())
        openProject(recent.first());
    else
        emit error("Создайте или откройте первый проект.");
}
void AppController::openPaths(const QStringList &paths, int line, int column) {
    startupLine = line; startupColumn = column;
    if (paths.isEmpty()) return;
    const QFileInfo first(paths.first());
    QString root = first.isDir() ? first.canonicalFilePath() : first.absolutePath();
    if (first.isFile()) {
        // Prefer the containing Python project, including nested source directories.
        QDir ancestor(root);
        do {
            if (QFileInfo::exists(PythonEnvironment::interpreter(ancestor.absolutePath()))) {
                root = ancestor.absolutePath(); break;
            }
        } while (ancestor.cdUp());
    }
    startupFiles.clear();
    for (const auto &path : paths)
        if (QFileInfo(path).isFile()) startupFiles << QFileInfo(path).canonicalFilePath();
    if (first.isFile() && !QFileInfo::exists(PythonEnvironment::interpreter(root))) {
        enterProject(root);
        if (first.suffix() == "py") emit environmentNeeded(root);
    } else openProject(root);
}
void AppController::openProject(const QString &value) {
    Log::write("ACTION", "openProject");
    QString p = value.startsWith("file:") ? QUrl(value).toLocalFile() : value;
    if (!QFileInfo(p).isDir()) {
        emit error("Папка проекта не найдена.");
        return;
    }
    if (!QFileInfo::exists(PythonEnvironment::interpreter(p))) {
        emit environmentNeeded(p);
        return;
    }
    enterProject(p);
}
void AppController::createProject(const QString &name, const QString &py, const QString &location) {
    Log::write("ACTION", "createProject");
    if (name.trimmed().isEmpty() || name == "." || name == ".." ||
        name.contains(QRegularExpression("[<>:\"/\\\\|?*]")) || name.endsWith('.') || name.endsWith(' ')) {
        emit error("Введите корректное название проекта.");
        return;
    }
    QString base = location.isEmpty()             ? projectsDirectory()
                   : location.startsWith("file:") ? QUrl(location).toLocalFile()
                                                  : location;
    QString path = QDir(base).absoluteFilePath(name);
    if (QFileInfo::exists(path)) {
        emit error("Проект с таким названием уже существует. Откройте его или выберите другое имя.");
        return;
    }
    if (py.isEmpty()) {
        emit error("Выберите Python или укажите путь к нему.");
        return;
    }
    currentStatus = "Создаём виртуальное окружение…";
    beginTask("python-env", QStringLiteral("Подготовка Python"), QStringLiteral("Создаю виртуальное окружение…"), 10);
    emit stateChanged();
    python.create(path, py);
}
void AppController::prepareEnvironment(const QString &path, const QString &py) {
    Log::write("ACTION", "prepareEnvironment");
    if (py.isEmpty()) {
        emit error("Выберите Python.");
        return;
    }
    beginTask("python-env", QStringLiteral("Подготовка Python"), QStringLiteral("Создаю виртуальное окружение…"), 10);
    python.create(path, py);
}
void AppController::addPython(const QString &url) {
    python.probe(url.startsWith("file:") ? QUrl(url).toLocalFile() : url);
}
void AppController::enterProject(const QString &path) {
    Log::write("ACTION", "enterProject");
    if (!project.isEmpty() && project != path) {
        if (!guard("openProject", path))
            return;
    }
    if (project != path) {
        persist();
        for (const auto &p : docs.order) {
            emit documentClosed(p);
        }
        docs.entries.clear();
        docs.order.clear();
        docs.active.clear();
        terminal.stop();
        language.stop();
        history.clear();
        currentProblems.clear();
        emit problemsChanged();
    }
    project = QFileInfo(path).canonicalFilePath();
    fs.setRoot(project);
    search.setProject(project, pythonPath());
    packageManager.configure(QFileInfo::exists(pythonPath()) ? pythonPath() : QString(), runtime("tools/package-helper.py"));
    assistant.configure(config.values, runtime("tools/ai-agent.py"), control.endpointPath, pythonPath());
    currentPage = "ide";
    beginTask("project-open", QStringLiteral("Открытие проекта"), QStringLiteral("Восстанавливаю редактор и файлы…"), 55);
    target = prefs.value("projects/" + project + "/target").toString();
    auto recent = prefs.value("recent").toStringList();
    recent.removeAll(project);
    recent.prepend(project);
    while (recent.size() > 20)
        recent.removeLast();
    prefs.setValue("recent", recent);
    auto opened = prefs.value("projects/" + project + "/opened").toStringList();
    for (const auto &p : opened)
        if (QFileInfo::exists(p) && fs.contains(p))
            docs.open(p);
    QString last = prefs.value("projects/" + project + "/active").toString();
    if (docs.entries.contains(last))
        docs.active = last;
    if (docs.order.isEmpty() && QFileInfo::exists(QDir(project).filePath("main.py")))
        docs.open(QDir(project).filePath("main.py"));
    for (const auto &file : startupFiles)
        if (fs.contains(file)) docs.open(file);
    startupFiles.clear();
    currentStatus = QFileInfo::exists(pythonPath()) ? "Подключаем Python…" : "Среда Python не настроена";
    emit stateChanged();
    emit documentsChanged();
    activate(startupLine, startupColumn);
    if (editorConnected) { startupLine = 0; startupColumn = 0; }
    if (QFileInfo::exists(pythonPath()))
    {
        beginTask("pyright", QStringLiteral("Python-анализатор"), QStringLiteral("Запускаю Pyright…"), 25);
        language.start(node(), runtime("node_modules/pyright/dist/pyright-langserver.js"), project, pythonPath());
    }
    finishTask("project-open", QStringLiteral("Проект открыт"));
}
void AppController::persist() {
    if (project.isEmpty())
        return;
    prefs.setValue("projects/" + project + "/opened", docs.order);
    prefs.setValue("projects/" + project + "/active", docs.active);
    prefs.setValue("projects/" + project + "/target", target);
    prefs.sync();
}
void AppController::activate(int line, int column) {
    currentProblems.clear();
    emit problemsChanged();
    if (docs.entries.contains(docs.active) && editorConnected)
        emit sourceActivated(docs.active, docs.entries[docs.active].text, line, column);
    emit documentsChanged();
}
void AppController::openFile(const QString &p, int line, int column) {
    Log::write("ACTION", "openFile");
    QString path = p.startsWith("file:") ? QUrl(p).toLocalFile() : p;
    QString err;
    if (!docs.open(path, &err)) {
        emit error(err);
        return;
    }
    activate(line, column);
    persist();
}
void AppController::editDocument(const QString &p, const QString &text) {
    docs.edit(p, text);
    search.updateDocument(p, text);
}
bool AppController::save() {
    QString err;
    bool ok = docs.active.isEmpty() || docs.save(docs.active, &err);
    if (!ok) emit error(err);
    else if (!docs.active.isEmpty()) language.fileChanged(docs.active);
    return ok;
}
bool AppController::saveAll() {
    QString err;
    bool ok = docs.saveAll(&err);
    if (!ok)
        emit error(err);
    if (ok) for (const auto &path : docs.order) language.fileChanged(path);
    return ok;
}
bool AppController::guard(const QString &action, const QString &p) {
    pendingAction = action;
    pendingPath = p;
    bool dirty = action == "close" ? docs.dirty(p) : docs.anyDirty();
    if (dirty) {
        emit confirmUnsaved("Есть несохранённые изменения. Сохранить их перед продолжением?");
        return false;
    }
    performPending();
    return false;
}
void AppController::performPending() {
    QString action = pendingAction, p = pendingPath;
    pendingAction.clear();
    pendingPath.clear();
    if (action == "close") {
        emit documentClosed(p);
        docs.close(p);
        activate();
        persist();
    } else if (action == "home") {
        persist();
        runner.stop();
        terminal.stop();
        language.stop();
        for (const auto &path : docs.order)
            emit documentClosed(path);
        docs.entries.clear();
        docs.order.clear();
        docs.active.clear();
        project.clear();
        search.setProject(QString(), QString());
        packageManager.configure(QString(), runtime("tools/package-helper.py"));
        currentPage = "welcome";
        emit stateChanged();
        emit documentsChanged();
    } else if (action == "quit") {
        persist();
        runner.stop();
        emit quitApproved();
    } else if (action == "openProject") {
        persist();
        runner.stop();
        terminal.stop();
        language.stop();
        for (const auto &path : docs.order)
            emit documentClosed(path);
        docs.entries.clear();
        docs.order.clear();
        docs.active.clear();
        project.clear();
        enterProject(p);
    }
}
void AppController::resolveUnsaved(const QString &choice) {
    emit unsavedResolved();
    if (choice == "cancel") {
        pendingAction.clear();
        pendingPath.clear();
        return;
    }
    if (choice == "save") {
        QString err;
        if (pendingAction == "close") {
            if (!docs.save(pendingPath, &err)) {
                emit error(err);
                return;
            }
        } else if (!saveAll())
            return;
    }
    performPending();
}
void AppController::closeTab(const QString &p) {
    Log::write("ACTION", "closeTab");
    guard("close", p);
}
void AppController::home() {
    Log::write("ACTION", "home");
    guard("home");
}
void AppController::settings() {
    Log::write("ACTION", "settings");
    previousPage = currentPage;
    currentPage = "settings";
    emit stateChanged();
}
void AppController::back() {
    Log::write("ACTION", "back");
    currentPage = previousPage;
    emit stateChanged();
}
void AppController::requestQuit() {
    Log::write("ACTION", "requestQuit");
    guard("quit");
}
void AppController::filterFiles(const QString &q) {
    fs.search(q);
}
void AppController::toggleFolder(const QString &p) {
    Log::write("ACTION", "toggleFolder");
    fs.toggle(p);
}
void AppController::fileOperation(const QString &op, const QString &path, const QString &name) {
    Log::write("FILE", op + " " + path);
    QString err;
    QString parent = QFileInfo(path).isDir() ? path : QFileInfo(path).absolutePath();
    if (op == "newFile")
        fs.create(parent, name, false, &err);
    else if (op == "newFolder")
        fs.create(parent, name, true, &err);
    else if (op == "rename") {
        if (fs.rename(path, name, &err)) {
            const QString replacement = QDir(QFileInfo(path).absolutePath()).filePath(name);
            relocateDocuments(path, replacement);
        }
    } else if (op == "delete") {
        QStringList affected;
        for (const auto &p : docs.order) {
            if (p == path || p.startsWith(path + "/")) {
                if (docs.dirty(p)) {
                    emit error("Сохраните или закройте изменённые файлы перед удалением.");
                    return;
                }
                affected << p;
            }
        }
        if (fs.remove(path, &err)) {
            for (const auto &p : affected) {
                emit documentClosed(p);
                docs.close(p);
            }
            activate();
            persist();
        }
    } else if (op == "copy" || op == "cut") {
        fs.clipboard = path;
        fs.cut = op == "cut";
    } else if (op == "paste")
        fs.paste(parent, &err);
    else if (op == "reveal")
        QDesktopServices::openUrl(QUrl::fromLocalFile(parent));
    if (!err.isEmpty())
        emit error(err);
}
void AppController::relocateDocuments(const QString &path, const QString &replacement) {
    const auto opened = docs.order;
    for (const auto &p : opened) {
        if (p != path && !p.startsWith(path + "/"))
            continue;
        const QString next = replacement + p.mid(path.size());
        emit documentClosed(p);
        Document d = docs.entries.take(p);
        d.path = next;
        docs.entries.insert(next, d);
        search.updateDocument(next, d.text);
        docs.order.replace(docs.order.indexOf(p), next);
        if (docs.active == p)
            docs.active = next;
    }
    if (target == path || target.startsWith(path + "/"))
        target = replacement + target.mid(path.size());
    activate();
    persist();
    emit stateChanged();
}
void AppController::setRunTarget(const QString &p) {
    Log::write("ACTION", "setRunTarget");
    target = p;
    persist();
    emit stateChanged();
}
void AppController::run() {
    if (running())
        return;
    if (config.values["general.saveBeforeRun"].toBool() && !saveAll())
        return;
    QString p = target.isEmpty() ? docs.active : target;
    if (!p.endsWith(".py", Qt::CaseInsensitive) || !QFileInfo::exists(p)) {
        emit error("Выберите Python-файл для запуска.");
        return;
    }
    if (!QFileInfo::exists(pythonPath())) {
        emit error("Виртуальное окружение недоступно.");
        return;
    }
    Log::write("RUN", QString("Start %1 (%2)").arg(p, config.values["console.output"].toString()));
    hasConsoleError = false;
    runner.run(pythonPath(), p, project, config.values["console.output"].toString() == "system");
}
void AppController::startTerminal() {
    Log::write("ACTION", "startTerminal");
    if (!project.isEmpty())
        terminal.start(node(), runtime("tools/terminal.cjs"), project, pythonPath());
}
void AppController::editorReady() {
    Log::write("ACTION", "editorReady");
    editorConnected = true;
    activate(startupLine, startupColumn);
    startupLine = 0; startupColumn = 0;
    if (language.ready)
        emit languageReady();
}
void AppController::setProblems(const QString &p, const QString &json) {
    if (p != docs.active)
        return;
    currentProblems = QJsonDocument::fromJson(json.toUtf8()).array().toVariantList();
    emit problemsChanged();
}
void AppController::navigate(const QString &p, int l, int c, const QString &from, int fl, int fc) {
    history.append(QVariantMap{{"path", from}, {"line", fl}, {"column", fc}});
    openFile(p, l, c);
}
void AppController::navigateBack() {
    Log::write("ACTION", "navigateBack");
    if (history.isEmpty())
        return;
    auto h = history.takeLast().toMap();
    openFile(h["path"].toString(), h["line"].toInt(), h["column"].toInt());
}
void AppController::command(const QString &name) {
    Log::write("COMMAND", name);
    if (name == "notifications")
        emit sidebarRequested(0);
    else if (name == "browser")
        emit sidebarRequested(1);
    else if (name == "save")
        save();
    else if (name == "saveAll")
        saveAll();
    else if (name == "close")
        closeTab(docs.active);
    else if (name == "run")
        run();
    else if (name == "quickOpen")
        emit showQuickOpen();
    else if (name == "back")
        navigateBack();
    else if (name == "terminal")
        emit panelRequested(1);
    else if (name == "problems")
        emit panelRequested(2);
    else if (name == "packages")
        emit panelRequested(3);
    else if (name == "console")
        emit panelRequested(0);
    else
        emit editorCommand(name);
}
void AppController::openBrowser(const QString &value) {
    Log::write("ACTION", "openBrowser");
    const QUrl url(value);
    if (!url.isValid() || !QStringList{"http", "https"}.contains(url.scheme())) {
        emit error("Разрешены только HTTP(S)-адреса.");
        return;
    }
    emit browserRequested(url.toString());
}
void AppController::installRequirements(const QString &path) {
    if (fs.contains(path) && QFileInfo(path).fileName() == "requirements.txt") {
        save();
        packageManager.installRequirements(path);
    }
}
void AppController::requestExternalLink(const QString &value) {
    const QUrl url(value);
    if (url.isValid() && QStringList{"http", "https"}.contains(url.scheme()))
        emit externalLinkRequested(url.toString());
}
void AppController::openExternalLink(const QString &value) {
    const QUrl url(value);
    if (url.isValid() && QStringList{"http", "https"}.contains(url.scheme()))
        QDesktopServices::openUrl(url);
}
QVariant AppController::pluginHost(const QStringList &args) {
    const QString executable = QFileInfo::exists(pythonPath()) ? pythonPath()
#ifdef Q_OS_WIN
        : QStandardPaths::findExecutable("python");
#else
        : QStandardPaths::findExecutable("python3");
#endif
    if (executable.isEmpty()) return QVariantMap{{"ok", false}, {"error", "Python не найден."}};
    QProcess process;
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONPATH", runtime("tools") + QDir::listSeparator() + env.value("PYTHONPATH"));
    env.insert("PYTHONIOENCODING", "utf-8");
    process.setProcessEnvironment(env);
    const QString root = QDir(QFileInfo(config.path).absolutePath()).filePath("plugins");
    process.start(executable, QStringList{runtime("tools/plugin-host.py"), root} + args);
    if (!process.waitForFinished(30000)) {
        process.kill();
        return QVariantMap{{"ok", false}, {"error", "Плагин не ответил вовремя."}};
    }
    const auto data = QJsonDocument::fromJson(process.readAllStandardOutput());
    if (data.isNull()) return QVariantMap{{"ok", false}, {"error", QString::fromUtf8(process.readAllStandardError())}};
    return data.toVariant();
}
void AppController::refreshPlugins() {
    const auto result = pluginHost({"list"});
    if (result.type() == QVariant::List) {
        pluginCatalog = result.toList();
        emit pluginsChanged();
    }
}
void AppController::installPlugin(const QString &source) {
    const auto result = pluginHost({"install", source}).toMap();
    if (!result["ok"].toBool()) emit error(result["error"].toString());
    refreshPlugins();
}
void AppController::togglePlugin(const QString &id, bool enabled) {
    const auto result = pluginHost({"toggle", id, enabled ? "1" : "0"}).toMap();
    if (!result["ok"].toBool()) emit error(result["error"].toString());
    refreshPlugins();
}
void AppController::removePlugin(const QString &id) {
    const auto result = pluginHost({"remove", id}).toMap();
    if (!result["ok"].toBool()) emit error(result["error"].toString());
    refreshPlugins();
}
void AppController::pluginAction(const QString &id, const QString &command) {
    const QVariantMap context{{"project", project}, {"activeFile", docs.active},
                              {"settings", config.values["extensions"]}};
    const auto result = pluginHost({"run", id, command, QString::fromUtf8(QJsonDocument::fromVariant(context).toJson(QJsonDocument::Compact))}).toMap();
    if (!result["ok"].toBool()) emit error(result["error"].toString());
    else emit pluginResult(id, result["result"].toString());
}
QVariantList AppController::pluginSearchResults(const QString &query) const {
    QVariantList results;
    for (const auto &value : pluginCatalog) {
        const auto plugin = value.toMap();
        if (!plugin["enabled"].toBool()) continue;
        for (const auto &entry : plugin["search"].toList()) {
            const auto item = entry.toMap();
            if (item["label"].toString().contains(query, Qt::CaseInsensitive))
                results << QVariantMap{{"group", plugin["name"]}, {"title", item["label"]},
                    {"detail", "Плагин"}, {"pluginId", plugin["id"]}, {"pluginCommand", item["command"]}};
        }
    }
    return results;
}
QString AppController::translate(const QString &source, const QString &locale) const {
    if (locale == "ru") return source;
    static QJsonObject english;
    static bool loaded = false;
    if (!loaded) {
        QFile file(runtime("assets/i18n/en.json"));
        if (file.open(QIODevice::ReadOnly)) english = QJsonDocument::fromJson(file.readAll()).object();
        loaded = true;
    }
    if (locale == "en" && english.contains(source)) return english[source].toString();
    for (const auto &value : pluginCatalog) {
        const auto plugin = value.toMap();
        if (!plugin["enabled"].toBool()) continue;
        const auto pack = plugin["translations"].toMap()[locale].toMap();
        if (pack.contains(source)) return pack[source].toString();
    }
    return source;
}
void AppController::openProjectLink(const QString &value) {
    QString link = value;
    if (link.startsWith("file://"))
        link = link.mid(7);
    int line = 0, column = 0;
    const QRegularExpression location("^(.*?):(\\d+)(?::(\\d+))?$");
    const auto match = location.match(link);
    if (match.hasMatch()) {
        link = match.captured(1);
        line = match.captured(2).toInt();
        column = match.captured(3).toInt();
    }
    const QString path = QFileInfo(link).isAbsolute() ? link : QDir(project).filePath(link);
    if (!fs.contains(path) || !QFileInfo(path).isFile()) {
        emit error("Ссылка ведёт за пределы проекта или файл не найден.");
        return;
    }
    openFile(path, line, qMax(1, column));
}
void AppController::askAi(const QString &text) {
    emit sidebarRequested(2);
    assistant.send(text);
}
void AppController::installOllama() {
    QDesktopServices::openUrl(QUrl("https://ollama.com/download"));
}

void AppController::copyText(const QString &text) {
    QGuiApplication::clipboard()->setText(text);
}
QString AppController::clipboardText() const {
    return QGuiApplication::clipboard()->text();
}

QString AppController::logPath() const {
    return Log::path();
}
bool AppController::setSetting(const QString &key, const QVariant &value) {
    Log::write("CONFIG", "Set " + key);
    QString message;
    if (!config.set(key, value, &message)) {
        emit error(message);
        return false;
    }
    return true;
}
void AppController::resetSettings() {
    Log::write("ACTION", "resetSettings");
    QString message;
    if (!config.patch(Configuration::defaults(), &message))
        emit error(message);
}
void AppController::setHotkey(const QString &name, const QString &sequence) {
    auto bindings = shortcuts();
    bindings[name] = sequence;
    setSetting("hotkeys", bindings);
}
void AppController::setEditorActions(const QString &json) {
    actionCatalog = QJsonDocument::fromJson(json.toUtf8()).array().toVariantList();
    emit editorActionsChanged();
}
bool AppController::notify(const QString &title, const QString &message) {
    if (title.trimmed().isEmpty() || title.size() > 300 || message.size() > 8000)
        return false;
    const auto delivery = config.values["notifications.delivery"].toString();
    Log::write("NOTIFICATION", QString("Notification delivered: %1").arg(delivery));
    if (delivery == "ide" || delivery == "both") {
        notificationHistory.prepend(QVariantMap{{"title", title},
                                                {"message", message},
                                                {"time", QDateTime::currentDateTime().toString("HH:mm:ss")}});
        while (notificationHistory.size() > 200)
            notificationHistory.removeLast();
        unread = qMin(200, unread + 1);
        emit notificationsChanged();
    }
    if (delivery == "system" || delivery == "both") {
        if (QSystemTrayIcon::isSystemTrayAvailable() && QSystemTrayIcon::supportsMessages())
            tray.showMessage(title, message, QSystemTrayIcon::Information, 6000);
        else {
            emit error("Системные уведомления недоступны в текущем окружении. Выберите уведомления IDE.");
            return false;
        }
    }
    return true;
}
void AppController::clearNotifications() {
    Log::write("ACTION", "clearNotifications");
    notificationHistory.clear();
    unread = 0;
    emit notificationsChanged();
}
void AppController::markNotificationsRead() {
    Log::write("ACTION", "markNotificationsRead");
    unread = 0;
    emit notificationsChanged();
}

void AppController::setEditorHotkey(const QString &name, const QString &sequence) {
    bool found = false;
    for (const auto &action : actionCatalog)
        if (action.toMap()["id"].toString() == name)
            found = true;
    if (!found) {
        emit error("Неизвестная команда редактора.");
        return;
    }
    auto map = config.values["editor.hotkeys"].toMap();
    map[name] = sequence;
    setSetting("editor.hotkeys", map);
}

QVariantList AppController::backgroundTasks() const {
    QVariantList result;
    for (auto it = tasks.cbegin(); it != tasks.cend(); ++it)
        result.append(it.value());
    return result;
}
int AppController::overallProgress() const {
    if (tasks.isEmpty()) return 100;
    int total = 0;
    for (const auto &task : tasks)
        total += qBound(0, task.value("progress").toInt(), 100);
    return total / tasks.size();
}
QString AppController::taskSummary() const {
    if (tasks.isEmpty()) return QStringLiteral("Готово");
    const auto task = tasks.cbegin().value();
    return tasks.size() == 1 ? task.value("detail").toString()
                             : QStringLiteral("Фоновые задачи: %1").arg(tasks.size());
}
void AppController::beginTask(const QString &id, const QString &title, const QString &detail, int progress) {
    tasks[id] = QVariantMap{{"id", id}, {"title", title}, {"detail", detail}, {"progress", progress},
                            {"startedMs", QDateTime::currentMSecsSinceEpoch()}};
    Log::write("TASK", QString("%1 started; progress=%2; %3").arg(id).arg(progress).arg(detail));
    emit tasksChanged();
}
void AppController::updateTask(const QString &id, const QString &detail, int progress) {
    if (!tasks.contains(id)) return;
    tasks[id]["detail"] = detail;
    tasks[id]["progress"] = progress;
    Log::write("TASK", QString("%1 progress=%2; %3").arg(id).arg(progress).arg(detail));
    emit tasksChanged();
}
void AppController::finishTask(const QString &id, const QString &detail) {
    if (!tasks.contains(id)) return;
    tasks[id]["progress"] = 100;
    if (!detail.isEmpty()) tasks[id]["detail"] = detail;
    const auto elapsed = QDateTime::currentMSecsSinceEpoch() - tasks[id].value("startedMs").toLongLong();
    Log::write("TASK", QString("%1 finished; elapsedMs=%2; %3").arg(id).arg(elapsed).arg(tasks[id].value("detail").toString()));
    emit tasksChanged();
    QTimer::singleShot(900, this, [this, id] {
        if (tasks.value(id).value("progress").toInt() == 100) {
            tasks.remove(id);
            emit tasksChanged();
        }
    });
}
void AppController::writeAutoSnapshot() {
    QVariantMap state{{"page", currentPage},
                      {"project", project},
                      {"activeFile", docs.active},
                      {"openTabs", docs.order},
                      {"dirtyDocuments", docs.anyDirty()},
                      {"python", pythonPath()},
                      {"runTarget", target},
                      {"running", runner.running()},
                      {"teacherRole", teacherSession.property("role")},
                      {"teacherStatus", teacherSession.property("status")},
                      {"teacherStudents", teacherSession.students()},
                      {"problems", currentProblems.size()},
                      {"packagesBusy", packageManager.busy()},
                      {"aiBusy", assistant.busy()},
                      {"tasks", backgroundTasks()},
                      {"status", currentStatus}};
    Log::writeSnapshot(QString::fromUtf8(QJsonDocument::fromVariant(state).toJson(QJsonDocument::Indented)));
    Log::write("SNAPSHOT", "AutoEDI.log updated");
}
