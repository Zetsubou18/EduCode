#include "PackageManager.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>

namespace {
QString normalizedPackageName(QString name) {
    name = name.toLower();
    name.replace(QRegularExpression("[-_.]+"), "-");
    return name;
}
}
PackageManager::PackageManager(QObject *parent) : QObject(parent) {
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("PYTHONIOENCODING", "utf-8");
    environment.insert("PYTHONUTF8", "1");
    process.setProcessEnvironment(environment);
    connect(&process, &QProcess::stateChanged, this, [this] { emit changed(); });
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError kind) {
        if (kind != QProcess::FailedToStart) return;
        currentStatus = "Ошибка запуска";
        emit error("Не удалось запустить менеджер пакетов: " + process.errorString());
        emit changed();
    });
    searchTimer.setSingleShot(true);
    searchTimer.setInterval(300);
    connect(&searchTimer, &QTimer::timeout, this, [this] { start("search", pendingQuery); });
    connect(
        &process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        [this](int code, QProcess::ExitStatus) {
            const auto finishedOperation = operation;
            const auto finishedArgument = operationArgument;
            const auto raw = process.readAllStandardOutput();
            const auto doc = QJsonDocument::fromJson(raw.trimmed());
            const auto errorText = doc.isObject() ? doc.object()["error"].toString() : QString();
            if (finishedOperation == "search" &&
                normalizedPackageName(finishedArgument) != normalizedPackageName(pendingQuery)) {
                continuePendingSearch();
                return;
            }
            if (code != 0 || !errorText.isEmpty()) {
                currentStatus = "Ошибка";
                emit error(errorText.isEmpty() ? QString::fromUtf8(process.readAllStandardError()).trimmed()
                                               : errorText);
            } else if (finishedOperation == "list") {
                installed = doc.array().toVariantList();
                packageItems = installed;
                currentStatus = "Установлено: " + QString::number(installed.size());
            } else if (finishedOperation == "search") {
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
                std::stable_sort(packageItems.begin(), packageItems.end(), [this](const QVariant &left, const QVariant &right) {
                    const auto needle = normalizedPackageName(pendingQuery);
                    const auto a = normalizedPackageName(left.toMap()["name"].toString());
                    const auto b = normalizedPackageName(right.toMap()["name"].toString());
                    const auto rank = [&needle](const QString &name) {
                        if (name == needle) return 0;
                        if (name.startsWith(needle)) return 1;
                        return 2;
                    };
                    if (rank(a) != rank(b)) return rank(a) < rank(b);
                    if (a.size() != b.size()) return a.size() < b.size();
                    return a < b;
                });
                currentStatus = packageItems.isEmpty() ? "Ничего не найдено" : "Результаты поиска";
            } else if (finishedOperation == "info") {
                packageDetails = doc.object().toVariantMap();
                currentStatus = packageDetails.isEmpty() ? "Ответ PyPI не распознан" : "Информация с PyPI";
            } else {
                currentStatus = doc.object()["message"].toString();
                emit packagesChanged();
                QTimer::singleShot(0, this, &PackageManager::refresh);
            }
            emit changed();
            if (finishedOperation == "list" && !pendingQuery.isEmpty())
                QTimer::singleShot(0, this, &PackageManager::continuePendingSearch);
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
    operationArgument = argument;
    currentStatus = action == "list"     ? "Читаю окружение…"
                    : action == "search" ? "Ищу на PyPI…"
                                         : "Выполняю операцию…";
    emit changed();
    QStringList args{helper, python, action};
    if (!argument.isEmpty())
        args << argument;
    process.start(python, args);
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
void PackageManager::continuePendingSearch() {
    if (!pendingQuery.isEmpty())
        start("search", pendingQuery);
}
void PackageManager::select(const QString &name) {
    selected = name;
    packageDetails.clear();
    start("info", name);
}
void PackageManager::install(const QString &name, const QString &version) {
    const auto target = version.trimmed().isEmpty() ? name : name + "==" + version.trimmed();
    start("install", target);
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
