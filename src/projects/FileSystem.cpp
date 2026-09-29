#include "FileSystem.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QtConcurrent>
namespace {
#ifdef Q_OS_WIN
constexpr auto pathSensitivity = Qt::CaseInsensitive;
#else
constexpr auto pathSensitivity = Qt::CaseSensitive;
#endif
}
FileSystem::FileSystem(QObject *p) : QObject(p) {
    connect(&transfer, &QFutureWatcher<bool>::finished, this, [this] {
        if (transfer.result()) {
            if (transferCut) {
                if (clipboard == transferSource)
                    clipboard.clear();
                emit pathMoved(transferSource, transferDestination);
            }
            emit transferFinished(QString());
        } else {
            emit transferFinished("Не удалось скопировать все файлы. Проверьте доступ к файлам.");
        }
        refresh();
    });
    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, [this] { refresh(); });
    connect(&scan, &QFutureWatcher<QVariantList>::finished, this, [this] {
        if (rescan) {
            rescan = false;
            refresh();
            return;
        }
        cache = scan.result();
        emit changed();
    });
}
void FileSystem::setRoot(const QString &p) {
    root = QFileInfo(p).canonicalFilePath();
    expanded.clear();
    query.clear();
    refresh();
}
bool FileSystem::contains(const QString &p) const {
    QString c = QFileInfo(p).canonicalFilePath();
    if (c.isEmpty())
        c = QFileInfo(QFileInfo(p).absolutePath()).canonicalFilePath() + "/" + QFileInfo(p).fileName();
    return !root.isEmpty() && (c == root || c.startsWith(root + "/", pathSensitivity));
}
void FileSystem::append(QVariantList &out, const QString &root, const QString &dir, int depth,
                        const QString &query, const QSet<QString> &expanded) {
    if (depth > 64 || out.size() > 20000)
        return;
    QDir d(dir);
    for (const auto &f : d.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot,
                                         QDir::DirsFirst | QDir::Name | QDir::IgnoreCase)) {
        if (f.fileName() == ".venv" || f.fileName() == ".git" || f.fileName() == "__pycache__" ||
            f.fileName() == ".educode" || f.isSymLink())
            continue;
        QString p = f.absoluteFilePath();
        bool match =
            query.isEmpty() || query == "*" || p.mid(root.size() + 1).contains(query, Qt::CaseInsensitive);
        if (match)
            out.append(QVariantMap{{"path", p},
                                   {"name", query.isEmpty() ? f.fileName() : p.mid(root.size() + 1)},
                                   {"directory", f.isDir()},
                                   {"depth", query.isEmpty() ? depth : 0},
                                   {"expanded", expanded.contains(p)},
                                   {"extension", f.suffix()}});
        if (f.isDir() && (!query.isEmpty() || expanded.contains(p)))
            append(out, root, p, depth + 1, query, expanded);
    }
}
QVariantList FileSystem::rows() const {
    return cache;
}
void FileSystem::search(const QString &text) {
    query = text;
    refresh();
}
void FileSystem::toggle(const QString &p) {
    if (expanded.contains(p))
        expanded.remove(p);
    else
        expanded.insert(p);
    refresh();
}
void FileSystem::refresh() {
    if (scan.isRunning()) {
        rescan = true;
        return;
    }
    if (!watcher.directories().isEmpty())
        watcher.removePaths(watcher.directories());
    QStringList dirs;
    if (!root.isEmpty())
        dirs << root;
    for (const auto &p : expanded)
        if (QFileInfo(p).isDir())
            dirs << p;
    if (!dirs.isEmpty())
        watcher.addPaths(dirs);
    const QString r = root, q = query;
    const QSet<QString> e = expanded;
    scan.setFuture(QtConcurrent::run([r, q, e] {
        QVariantList out;
        if (!r.isEmpty())
            append(out, r, r, 0, q, e);
        return out;
    }));
}
static bool validName(const QString &n) {
    return !n.trimmed().isEmpty() && n != "." && n != ".." &&
           !n.contains(QRegularExpression("[<>:\"/\\\\|?*\\x00-\\x1f]")) && !n.endsWith('.') &&
           !n.endsWith(' ');
}
bool FileSystem::create(const QString &parent, const QString &name, bool directory, QString *error) {
    if (!contains(parent) || !validName(name)) {
        *error = "Некорректное имя или путь.";
        return false;
    }
    QString p = QDir(parent).filePath(name);
    if (QFileInfo::exists(p)) {
        *error = "Имя уже занято.";
        return false;
    }
    bool ok;
    if (directory)
        ok = QDir().mkdir(p);
    else {
        QFile f(p);
        ok = f.open(QIODevice::WriteOnly);
        if (!ok)
            *error = f.errorString();
    }
    if (!ok && error->isEmpty())
        *error = "Не удалось создать объект.";
    refresh();
    return ok;
}
bool FileSystem::rename(const QString &p, const QString &name, QString *error) {
    if (!contains(p) || p == root || !validName(name)) {
        *error = "Некорректное имя.";
        return false;
    }
    bool ok = QDir().rename(p, QDir(QFileInfo(p).absolutePath()).filePath(name));
    if (!ok)
        *error = "Не удалось переименовать. Проверьте имя и доступ.";
    refresh();
    return ok;
}
bool FileSystem::remove(const QString &p, QString *error) {
    if (!contains(p) || p == root) {
        *error = "Нельзя удалить этот путь.";
        return false;
    }
    bool ok = QFile::moveToTrash(p);
    if (!ok)
        *error = "Не удалось переместить в корзину.";
    refresh();
    return ok;
}
bool FileSystem::copyTree(const QString &from, const QString &to) {
    QFileInfo f(from);
    if (f.isSymLink())
        return false;
    if (f.isFile())
        return QFile::copy(from, to);
    if (!QDir().mkdir(to))
        return false;
    for (const auto &c : QDir(from).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden))
        if (!copyTree(c.absoluteFilePath(), QDir(to).filePath(c.fileName())))
            return false;
    return true;
}
bool FileSystem::paste(const QString &parent, QString *error) {
    if (transfer.isRunning()) {
        *error = "Дождитесь завершения предыдущего копирования.";
        return false;
    }
    QString to = QDir(parent).filePath(QFileInfo(clipboard).fileName());
    if (!contains(parent) || !contains(clipboard) || clipboard.isEmpty() || QFileInfo::exists(to) ||
        to.startsWith(clipboard + "/", pathSensitivity)) {
        *error = "Вставка невозможна: имя занято или папка вложена в себя.";
        return false;
    }
    transferSource = clipboard;
    transferDestination = to;
    transferCut = cut;
    const QString from = clipboard;
    const bool move = cut;
    transfer.setFuture(
        QtConcurrent::run([from, to, move] { return move ? QDir().rename(from, to) : copyTree(from, to); }));
    return true;
}
