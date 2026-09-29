#pragma once
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QObject>
#include <QSet>
#include <QVariantList>
class FileSystem : public QObject {
    Q_OBJECT
  public:
    explicit FileSystem(QObject *p = nullptr);
    QString root, query;
    QSet<QString> expanded;
    QString clipboard;
    bool cut = false;
    void setRoot(const QString &path);
    QVariantList rows() const;
    bool contains(const QString &path) const;
    bool create(const QString &parent, const QString &name, bool directory, QString *error);
    bool rename(const QString &path, const QString &name, QString *error);
    bool remove(const QString &path, QString *error);
    bool paste(const QString &parent, QString *error);
    void toggle(const QString &path);
    void search(const QString &text);
    void refresh();
  signals:
    void changed();
    void pathMoved(const QString &from, const QString &to);
    void transferFinished(const QString &error);

  private:
    QFileSystemWatcher watcher;
    QFutureWatcher<QVariantList> scan;
    QFutureWatcher<bool> transfer;
    QString transferSource, transferDestination;
    bool transferCut = false;
    QVariantList cache;
    bool rescan = false;
    static void append(QVariantList &result, const QString &root, const QString &directory, int depth,
                       const QString &query, const QSet<QString> &expanded);
    static bool copyTree(const QString &from, const QString &to);
};
