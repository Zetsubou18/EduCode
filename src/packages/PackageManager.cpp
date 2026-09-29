#include "PackageManager.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QFileInfo>
PackageManager::PackageManager(QObject *parent) : QObject(parent) {
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("PYTHONIOENCODING", "utf-8");
    environment.insert("PYTHONUTF8", "1");
    process.setProcessEnvironment(environment);
    connect(&process, &QProcess::stateChanged, this, [this] { emit changed(); });
    searchTimer.setSingleShot(true);
    searchTimer.setInterval(350);
    connect(&searchTimer, &QTimer::timeout, this, [this] { start("search", pendingQuery); });
    connect(
        &process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        [this](int code, QProcess::ExitStatus) {
            const auto raw = process.readAllStandardOutput();
            const auto doc = QJsonDocument::fromJson(raw.trimmed());
            const auto errorText = doc.isObject() ? doc.object()["error"].toString() : QString();
            if (code != 0 || !errorText.isEmpty()) {
                currentStatus = "Ошибка";
                emit error(errorText.isEmpty() ? QString::fromUtf8(process.readAllStandardError()).trimmed()
                                               : errorText);
            } else if (operation == "list") {
                installed = doc.array().toVariantList();
                packageItems = installed;
                currentStatus = "Установлено: " + QString::number(installed.size());
            } else if (operation == "search") {
                packageItems.clear();
                for (const auto &present : installed)
                    if (present.toMap()["name"].toString().contains(pendingQuery, Qt::CaseInsensitive))
                        packageItems.append(present);
                for (const auto &entry : doc.array().toVariantList()) {
                    auto candidate = entry.toMap();
                    bool found = false;
                    for (const auto &present : installed)
                        if (present.toMap()["name"].toString().compare(candidate["name"].toString(),
                                                                       Qt::CaseInsensitive) == 0)
                            found = true;
                    if (!found)
                        packageItems.append(candidate);
                }
                currentStatus = packageItems.isEmpty() ? "Ничего не найдено" : "Результаты поиска";
            } else if (operation == "info") {
                packageDetails = doc.object().toVariantMap();
                currentStatus = packageDetails.isEmpty() ? "Ответ PyPI не распознан" : "Информация с PyPI";
            } else {
                currentStatus = doc.object()["message"].toString();
                emit packagesChanged();
                QTimer::singleShot(0, this, &PackageManager::refresh);
            }
            emit changed();
            if (operation == "list" && !pendingQuery.isEmpty())
                QTimer::singleShot(0, this, [this] { start("search", pendingQuery); });
        });
}
PackageManager::~PackageManager() {
    if (busy()) {
        process.kill();
        process.waitForFinished(1000);
    }
}
void PackageManager::configure(const QString &p, const QString &h) {
    python = p;
    helper = h;
    packageItems.clear();
    installed.clear();
    packageDetails.clear();
    emit changed();
    if (!p.isEmpty())
        refresh();
}
void PackageManager::start(const QString &action, const QString &argument) {
    if (busy() || python.isEmpty())
        return;
    operation = action;
    currentStatus = action == "list"     ? "Читаю окружение…"
                    : action == "search" ? "Ищу на PyPI…"
                                         : "Выполняю операцию…";
    emit changed();
    QStringList args{helper, python, action};
    if (!argument.isEmpty())
        args << argument;
    process.start(python, args);
    if (!process.waitForStarted(3000)) {
        currentStatus = "Ошибка запуска";
        emit error("Не удалось запустить менеджер пакетов: " + process.errorString());
        emit changed();
    }
    emit changed();
}
void PackageManager::refresh() {
    start("list");
}
void PackageManager::search(const QString &q) {
    pendingQuery = q.trimmed();
    if (pendingQuery.isEmpty()) {
        searchTimer.stop();
        packageItems = installed;
        currentStatus = "Установлено: " + QString::number(installed.size());
        emit changed();
    } else
        searchTimer.start();
}
void PackageManager::select(const QString &name) {
    selected = name;
    packageDetails.clear();
    start("info", name);
}
void PackageManager::install(const QString &name) {
    start("install", name);
}
void PackageManager::installRequirements(const QString &path) {
    if (QFileInfo(path).fileName() == "requirements.txt" && QFileInfo(path).isFile())
        start("requirements", path);
}
void PackageManager::update(const QString &name) {
    start("update", name);
}
void PackageManager::remove(const QString &name) {
    start("remove", name);
}
