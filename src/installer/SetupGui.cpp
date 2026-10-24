#include <QApplication>
#include "Version.h"
#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QSettings>
#include <QVBoxLayout>
#include <QWidget>
#include <QtConcurrent>
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>

namespace {
struct OperationResult {
    bool okay = false;
    QString error;
};
QString defaultPath() {
    return QDir(qEnvironmentVariable("LOCALAPPDATA")).filePath("Programs/EduCode");
}
struct ExistingInstallation {
    QString path;
    QString version;
};
ExistingInstallation existingInstallation() {
    QSettings registry("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\EduCode",
                       QSettings::NativeFormat);
    ExistingInstallation result{QDir::cleanPath(registry.value("InstallLocation").toString()),
                                registry.value("DisplayVersion").toString()};
    if (result.path.isEmpty() || !QFileInfo(result.path + "/EduCode.exe").isFile()) {
        result.path = defaultPath();
        result.version.clear();
        if (!QFileInfo(result.path + "/EduCode.exe").isFile())
            result = {};
    }
    return result;
}
bool shortcut(const QString &path, const QString &target) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IShellLinkW *link = nullptr;
    bool okay = false;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                   reinterpret_cast<void **>(&link)))) {
        link->SetPath(reinterpret_cast<LPCWSTR>(target.utf16()));
        link->SetWorkingDirectory(reinterpret_cast<LPCWSTR>(QFileInfo(target).absolutePath().utf16()));
        link->SetDescription(L"EduCode Python IDE");
        IPersistFile *persist = nullptr;
        if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&persist)))) {
            okay = SUCCEEDED(persist->Save(reinterpret_cast<LPCWSTR>(path.utf16()), TRUE));
            persist->Release();
        }
        link->Release();
    }
    CoUninitialize();
    return okay;
}
QString startLink() { return QDir(qEnvironmentVariable("APPDATA")).filePath("Microsoft/Windows/Start Menu/Programs/EduCode.lnk"); }
QString desktopLink() { return QDir(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).filePath("EduCode.lnk"); }
void registerApp(const QString &folder, const QString &launcher) {
    HKEY key = nullptr;
    const wchar_t *name = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\EduCode";
    if (RegCreateKeyExW(HKEY_CURRENT_USER, name, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return;
    auto write = [key](const wchar_t *field, const QString &value) {
        const auto data = reinterpret_cast<const BYTE *>(value.utf16());
        RegSetValueExW(key, field, 0, REG_SZ, data, DWORD((value.size() + 1) * sizeof(wchar_t)));
    };
    write(L"DisplayName", "EduCode"); write(L"DisplayVersion", EDUCODE_VERSION);
    write(L"Publisher", "Zetsubou"); write(L"InstallLocation", folder);
    write(L"DisplayIcon", QDir(folder).filePath("EduCode.exe"));
    write(L"UninstallString", QString("\"%1\" --uninstall \"%2\"").arg(launcher, folder));
    write(L"QuietUninstallString", QString("\"%1\" --uninstall-silent \"%2\"").arg(launcher, folder));
    RegCloseKey(key);
}
void unregisterApp() {
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\EduCode");
}
bool validTarget(const QString &path) {
    const QString cleaned = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (cleaned.isEmpty() || QDir(cleaned).isRoot()) return false;
    const QString windows = QDir::cleanPath(qEnvironmentVariable("WINDIR"));
    return !cleaned.startsWith(windows, Qt::CaseInsensitive);
}
bool unpack(const QString &archive, const QString &target, QString *error) {
    QDir().mkpath(target);
    QProcess process;
    process.start("tar.exe", {"-xf", archive, "-C", target});
    if (!process.waitForStarted(5000) || !process.waitForFinished(600000) || process.exitCode() != 0) {
        *error = QString::fromUtf8(process.readAllStandardError()).trimmed();
        if (error->isEmpty()) *error = "Не удалось распаковать приложение. Требуется Windows 10/11 с tar.exe.";
        return false;
    }
    return true;
}
bool install(const QString &bundle, const QString &folder, const QString &launcher,
             bool desktop, bool startMenu, QString *error) {
    if (!validTarget(folder) || !QFileInfo(bundle + "/payload.zip").isFile()) {
        *error = "Некорректная папка установки или архив приложения."; return false;
    }
    const bool updating = QFileInfo(folder + "/EduCode.exe").isFile();
    if (QDir(folder).exists() && !updating &&
        !QDir(folder).entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty()) {
        *error = "Папка не пуста. Выберите другую папку или удалите предыдущую установку."; return false;
    }
    QString backup;
    if (updating) {
        backup = folder + ".update-backup-" + QString::number(QDateTime::currentMSecsSinceEpoch());
        if (!QDir().rename(folder, backup)) {
            *error = "Не удалось подготовить обновление. Закройте EduCode и повторите попытку.";
            return false;
        }
    }
    auto rollback = [&] {
        if (QDir(folder).exists())
            QDir(folder).removeRecursively();
        if (!backup.isEmpty())
            QDir().rename(backup, folder);
    };
    if (!unpack(bundle + "/payload.zip", folder, error)) { rollback(); return false; }
    if (!QFileInfo(folder + "/EduCode.exe").isFile() || !QFile::copy(launcher, folder + "/Uninstall.exe")) {
        *error = "Не удалось завершить установку EduCode."; rollback(); return false;
    }
    const QString executable = folder + "/EduCode.exe";
    if (!qEnvironmentVariableIsSet("EDUCODE_SETUP_TEST_MODE")) {
        if (startMenu) {
            if (!shortcut(startLink(), executable)) { *error = "Не удалось создать пункт меню Пуск."; rollback(); return false; }
        } else QFile::remove(startLink());
        if (desktop) {
            if (!shortcut(desktopLink(), executable)) { *error = "Не удалось создать значок на рабочем столе."; rollback(); return false; }
        } else QFile::remove(desktopLink());
        registerApp(folder, folder + "/Uninstall.exe");
    }
    if (!backup.isEmpty())
        QDir(backup).removeRecursively();
    return true;
}
bool uninstall(const QString &folder, QString *error) {
    if (!validTarget(folder) || !QFileInfo(folder + "/EduCode.exe").isFile()) {
        *error = "Установка EduCode не найдена."; return false;
    }
    if (!qEnvironmentVariableIsSet("EDUCODE_SETUP_TEST_MODE")) {
        QFile::remove(startLink()); QFile::remove(desktopLink());
        unregisterApp();
    }
    if (!QDir(folder).removeRecursively()) { *error = "Не удалось удалить папку программы. Закройте EduCode и повторите."; return false; }
    return true;
}
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("EduCode Setup");
    const QStringList args = app.arguments();
    const int bundleIndex = args.indexOf("--bundle");
    const int launcherIndex = args.indexOf("--launcher");
    const QString bundle = bundleIndex >= 0 ? args.value(bundleIndex + 1) : QString();
    const QString launcher = launcherIndex >= 0 ? args.value(launcherIndex + 1) : QString();
    const bool removing = args.contains("--uninstall") || args.contains("--uninstall-silent");
    const bool silent = args.contains("--install-silent") || args.contains("--uninstall-silent");
    const auto existing = existingInstallation();
    QString target = existing.path.isEmpty() ? defaultPath() : existing.path;
    if (removing) target = args.value(args.indexOf(args.contains("--uninstall") ? "--uninstall" : "--uninstall-silent") + 1);
    if (args.contains("--install-silent")) target = args.value(args.indexOf("--install-silent") + 1);
    QString error;
    if (silent) {
        const bool okay = removing ? uninstall(target, &error) : install(bundle, target, launcher, false, true, &error);
        if (!okay) fprintf(stderr, "%s\n", error.toUtf8().constData());
        return okay ? 0 : 1;
    }
    const bool updating = !removing && QFileInfo(target + "/EduCode.exe").isFile();
    const bool sameVersion = updating && existing.version == QStringLiteral(EDUCODE_VERSION);
    QWidget window;
    window.setWindowTitle(removing ? "Удаление EduCode" : updating ? "Обновление EduCode" : "Установка EduCode");
    window.setMinimumWidth(460);
    auto *layout = new QVBoxLayout(&window);
    auto *heading = new QLabel(removing ? "Удалить EduCode?" : updating
        ? (sameVersion ? QStringLiteral("Переустановить EduCode %1").arg(EDUCODE_VERSION)
                       : QStringLiteral("Обновить EduCode до %1").arg(EDUCODE_VERSION))
        : QStringLiteral("Установить EduCode %1").arg(EDUCODE_VERSION));
    QFont font = heading->font(); font.setPointSize(17); font.setBold(true); heading->setFont(font);
    layout->addWidget(heading);
    layout->addWidget(new QLabel(removing ? "Программа и её ярлыки будут удалены. Ваши проекты останутся." : updating
        ? QStringLiteral("Найдена установленная версия %1. Старые файлы будут заменены автоматически; проекты и настройки сохранятся.").arg(existing.version.isEmpty() ? "EduCode" : existing.version)
        : "Выберите папку установки. Проекты и настройки хранятся отдельно."));
    auto *path = new QLineEdit(target); path->setReadOnly(removing || updating);
    auto *row = new QHBoxLayout; row->addWidget(path);
    auto *browse = new QPushButton("Обзор…"); browse->setVisible(!removing && !updating); row->addWidget(browse);
    layout->addLayout(row);
    auto *startMenu = new QCheckBox("Добавить в меню Пуск"); startMenu->setChecked(!updating || QFileInfo::exists(startLink()));
    auto *desktop = new QCheckBox("Создать ярлык на рабочем столе");
    desktop->setChecked(updating && QFileInfo::exists(desktopLink()));
    startMenu->setVisible(!removing); desktop->setVisible(!removing);
    layout->addWidget(startMenu); layout->addWidget(desktop);
    auto *hint = new QLabel("Для закрепления на панели задач щёлкните правой кнопкой значок EduCode после установки.");
    hint->setWordWrap(true); hint->setVisible(!removing); layout->addWidget(hint);
    auto *progress = new QProgressBar; progress->setRange(0, 0); progress->hide(); layout->addWidget(progress);
    auto *status = new QLabel; status->setWordWrap(true); layout->addWidget(status);
    auto *action = new QPushButton(removing ? "Удалить" : updating ? (sameVersion ? "Переустановить" : "Обновить") : "Установить"); layout->addWidget(action);
    QObject::connect(browse, &QPushButton::clicked, [&] {
        const auto value = QFileDialog::getExistingDirectory(&window, "Папка установки", QFileInfo(path->text()).absolutePath());
        if (!value.isEmpty()) path->setText(QDir(value).filePath("EduCode"));
    });
    QObject::connect(action, &QPushButton::clicked, [&] {
        const auto targetPath = path->text();
        const auto createDesktop = desktop->isChecked();
        const auto createStartMenu = startMenu->isChecked();
        action->setEnabled(false); browse->setEnabled(false); path->setEnabled(false);
        desktop->setEnabled(false); startMenu->setEnabled(false);
        progress->show(); status->setText(removing ? "Удаление файлов…" : updating ? "Замена файлов предыдущей версии…" : "Распаковка и установка файлов…");
        auto *watcher = new QFutureWatcher<OperationResult>(&window);
        QObject::connect(watcher, &QFutureWatcher<OperationResult>::finished, &window, [&, watcher] {
            const auto result = watcher->result();
            watcher->deleteLater();
            progress->hide(); action->setEnabled(true); browse->setEnabled(true); path->setEnabled(true);
            desktop->setEnabled(true); startMenu->setEnabled(true);
            if (result.okay) {
                QMessageBox::information(&window, "EduCode", removing ? "EduCode удалён." : updating ? "EduCode обновлён." : "EduCode установлен.");
                app.quit();
            } else {
                status->setText(result.error);
                QMessageBox::warning(&window, "EduCode", result.error);
            }
        });
        watcher->setFuture(QtConcurrent::run([=] {
            OperationResult result;
            result.okay = removing ? uninstall(targetPath, &result.error)
                                     : install(bundle, targetPath, launcher, createDesktop, createStartMenu, &result.error);
            return result;
        }));
    });
    window.show();
    return app.exec();
}
