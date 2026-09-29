#pragma once
#include "Commands.h"
#include "Configuration.h"
#include "ControlServer.h"
#include "Documents.h"
#include "TeacherSession.h"
#include "ai/AiAssistant.h"
#include "language/LanguageServer.h"
#include "packages/PackageManager.h"
#include "platform/TerminalSession.h"
#include "process/ProcessRunner.h"
#include "projects/FileSystem.h"
#include "projects/PythonEnvironment.h"
#include "search/SearchService.h"
#include <QObject>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QVariantList>
class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchChanged)
    Q_PROPERTY(QVariantList runTargets READ runTargets NOTIFY filesChanged)
    Q_PROPERTY(QString page READ page NOTIFY stateChanged)
    Q_PROPERTY(QString projectName READ projectName NOTIFY stateChanged)
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY stateChanged)
    Q_PROPERTY(QString activePath READ activePath NOTIFY documentsChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString pythonPath READ pythonPath NOTIFY stateChanged)
    Q_PROPERTY(QString runTarget READ runTarget NOTIFY stateChanged)
    Q_PROPERTY(QVariantList recentProjects READ recentProjects NOTIFY stateChanged)
    Q_PROPERTY(QVariantList pythonVersions READ pythonVersions NOTIFY stateChanged)
    Q_PROPERTY(QVariantList tabs READ tabs NOTIFY documentsChanged)
    Q_PROPERTY(QVariantList files READ files NOTIFY filesChanged)
    Q_PROPERTY(QVariantList problems READ problems NOTIFY problemsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(QString projectsDirectory READ projectsDirectory NOTIFY configurationChanged)
    Q_PROPERTY(QVariantMap shortcuts READ shortcuts NOTIFY configurationChanged)
    Q_PROPERTY(QVariantMap configuration READ configuration NOTIFY configurationChanged)
    Q_PROPERTY(QString configPath READ configPath CONSTANT)
    Q_PROPERTY(QString logPath READ logPath CONSTANT)
    Q_PROPERTY(QVariantList notifications READ notifications NOTIFY notificationsChanged)
    Q_PROPERTY(int unreadNotifications READ unreadNotifications NOTIFY notificationsChanged)
    Q_PROPERTY(QVariantList editorActions READ editorActions NOTIFY editorActionsChanged)
    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)
    Q_PROPERTY(QObject *packages READ packages CONSTANT)
    Q_PROPERTY(QObject *ai READ ai CONSTANT)
    Q_PROPERTY(QObject *teacher READ teacher CONSTANT)
    Q_PROPERTY(bool consoleHasError READ consoleHasError NOTIFY stateChanged)
  public:
    TeacherSession *teacher() { return &teacherSession; }
    qint64 demoProcessId() const { return runner.processId(); }
    QObject *packages() {
        return &packageManager;
    }
    QObject *ai() {
        return &assistant;
    }
    bool consoleHasError() const {
        return hasConsoleError;
    }
    QVariantMap configuration() const {
        return config.values;
    }
    QString configPath() const {
        return config.path;
    }
    QString logPath() const;
    QVariantList notifications() const {
        return notificationHistory;
    }
    int unreadNotifications() const {
        return unread;
    }
    QVariantList editorActions() const {
        return actionCatalog;
    }
    QVariantList plugins() const { return pluginCatalog; }
    Q_INVOKABLE void refreshPlugins();
    Q_INVOKABLE void installPlugin(const QString &source);
    Q_INVOKABLE void togglePlugin(const QString &id, bool enabled);
    Q_INVOKABLE void removePlugin(const QString &id);
    Q_INVOKABLE void pluginAction(const QString &id, const QString &command);
    Q_INVOKABLE QVariantList pluginSearchResults(const QString &query) const;
    Q_INVOKABLE QString translate(const QString &source, const QString &locale) const;
    Q_INVOKABLE bool setSetting(const QString &key, const QVariant &value);
    Q_INVOKABLE bool notify(const QString &title, const QString &message);
    Q_INVOKABLE void clearNotifications();
    Q_INVOKABLE void markNotificationsRead();
    Q_INVOKABLE void setEditorActions(const QString &json);
    Q_INVOKABLE void setHotkey(const QString &command, const QString &sequence);
    Q_INVOKABLE void setEditorHotkey(const QString &command, const QString &sequence);
    Q_INVOKABLE void resetSettings();
    QVariantList searchResults() const {
        return search.results;
    }
    QVariantList runTargets() const {
        return search.runTargets;
    }
    Q_INVOKABLE void searchQuery(const QString &text) {
        search.query(text);
    }
    Q_INVOKABLE void copyText(const QString &text);
    Q_INVOKABLE QString clipboardText() const;
    explicit AppController(QObject *p = nullptr);
    QVariantMap shortcuts() const {
        return config.values["hotkeys"].toMap();
    }
    QString page() const {
        return currentPage;
    }
    QString projectName() const;
    QString projectPath() const {
        return project;
    }
    QString activePath() const {
        return docs.active;
    }
    QString status() const {
        return currentStatus;
    }
    QString pythonPath() const {
        return PythonEnvironment::interpreter(project);
    }
    QString runTarget() const {
        return target;
    }
    QString projectsDirectory() const;
    QVariantList recentProjects() const;
    QVariantList pythonVersions() const {
        return python.versions;
    }
    QVariantList tabs() const {
        return docs.tabs();
    }
    QVariantList files() const {
        return fs.rows();
    }
    QVariantList problems() const {
        return currentProblems;
    }
    bool busy() const {
        return python.busy;
    }
    bool running() const {
        return runner.running();
    }
    void openPaths(const QStringList &paths, int line = 0, int column = 0);
    Q_INVOKABLE void openProject(const QString &path);
    Q_INVOKABLE void continueProject();
    Q_INVOKABLE void createProject(const QString &name, const QString &pythonPath,
                                   const QString &location = QString());
    Q_INVOKABLE void prepareEnvironment(const QString &path, const QString &pythonPath);
    Q_INVOKABLE void addPython(const QString &url);
    Q_INVOKABLE void openFile(const QString &path, int line = 0, int column = 0);
    Q_INVOKABLE void editDocument(const QString &path, const QString &text);
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool saveAll();
    Q_INVOKABLE void closeTab(const QString &path);
    Q_INVOKABLE void home();
    Q_INVOKABLE void settings();
    Q_INVOKABLE void back();
    Q_INVOKABLE void requestQuit();
    Q_INVOKABLE void resolveUnsaved(const QString &choice);
    Q_INVOKABLE void filterFiles(const QString &query);
    Q_INVOKABLE void toggleFolder(const QString &path);
    Q_INVOKABLE void fileOperation(const QString &operation, const QString &path,
                                   const QString &name = QString());
    Q_INVOKABLE void setRunTarget(const QString &path);
    Q_INVOKABLE void run();
    Q_INVOKABLE void stop() {
        runner.stop();
    }
    Q_INVOKABLE void consoleInput(const QString &text) {
        runner.input(text);
    }
    Q_INVOKABLE void startTerminal();
    Q_INVOKABLE void terminalInput(const QString &text) {
        terminal.input(text);
    }
    Q_INVOKABLE void terminalResize(int columns, int rows) {
        terminal.resize(columns, rows);
    }
    Q_INVOKABLE void lspSend(const QString &message) {
        language.relay(message);
    }
    Q_INVOKABLE void editorReady();
    Q_INVOKABLE void setProblems(const QString &path, const QString &json);
    Q_INVOKABLE void navigate(const QString &path, int line, int column, const QString &from, int fromLine,
                              int fromColumn);
    Q_INVOKABLE void navigateBack();
    Q_INVOKABLE void command(const QString &name);
    Q_INVOKABLE void openBrowser(const QString &url);
    Q_INVOKABLE void openProjectLink(const QString &link);
    Q_INVOKABLE void installRequirements(const QString &path);
    Q_INVOKABLE void requestExternalLink(const QString &url);
    Q_INVOKABLE void openExternalLink(const QString &url);
    Q_INVOKABLE void askAi(const QString &text);
    Q_INVOKABLE void installOllama();
  signals:
    void configurationChanged();
    void notificationsChanged();
    void editorActionsChanged();
    void pluginsChanged();
    void pluginResult(const QString &id, const QString &result);
    void sidebarRequested(int index);
    void browserRequested(const QString &url);
    void externalLinkRequested(const QString &url);
    void searchChanged();
    void stateChanged();
    void documentsChanged();
    void filesChanged();
    void problemsChanged();
    void sourceActivated(const QString &path, const QString &text, int line, int column);
    void documentClosed(const QString &path);
    void editorCommand(const QString &name);
    void panelRequested(int index);
    void consoleOutput(const QString &text);
    void terminalOutput(const QString &text);
    void lspMessage(const QString &json);
    void languageReady();
    void error(const QString &message);
    void confirmUnsaved(const QString &message);
    void unsavedResolved();
    void quitApproved();
    void environmentNeeded(const QString &path);
    void showQuickOpen();

  private:
    Configuration config;
    ControlServer control;
    QSystemTrayIcon tray;
    QVariantList notificationHistory, actionCatalog;
    QVariantList pluginCatalog;
    int unread = 0;
    SearchService search;
    Documents docs;
    PythonEnvironment python;
    FileSystem fs;
    ProcessRunner runner;
    LanguageServer language;
    TerminalSession terminal;
    PackageManager packageManager;
    AiAssistant assistant;
    QSettings prefs;
    QString project, currentPage = "welcome", previousPage = "welcome", currentStatus = "Готово", target;
    QString pendingAction, pendingPath;
    QStringList startupFiles;
    int startupLine = 0, startupColumn = 0;
    QVariantList currentProblems, history;
    TeacherSession teacherSession{this};
    bool editorConnected = false;
    QString consoleBuffer, terminalBuffer;
    bool hasConsoleError = false;
    QString node() const;
    QString runtime(const QString &relative) const;
    void activate(int line = 0, int column = 0);
    void enterProject(const QString &path);
    void persist();
    void relocateDocuments(const QString &from, const QString &to);
    void performPending();
    bool guard(const QString &action, const QString &path = QString());
    QVariant pluginHost(const QStringList &args);
};
