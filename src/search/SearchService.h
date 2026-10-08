#pragma once
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <functional>
class SearchService : public QObject {
    Q_OBJECT
  public:
    using Provider = std::function<QVariantList(const QString &)>;
    explicit SearchService(QObject *parent = nullptr);
    ~SearchService() override;
    void setProject(const QString &root, const QString &python);
    void refresh();
    void refreshLibraries();
    void query(const QString &text);
    void updateDocument(const QString &path, const QString &text);
    void forgetDocument(const QString &path);
    void registerProvider(const QString &id, Provider provider);
    QVariantList results, runTargets;
  signals:
    void changed();
    void indexChanged();
    void scanStarted();
    void scanFinished(int files);
    void libraryIndexStarted();
    void libraryIndexFinished(int entries, bool ok);

  private:
    QString root, expression, interpreter, scannedRoot;
    QVariantList projectFiles, libraries;
    QList<QPair<QString, Provider>> providers;
    QFutureWatcher<QVariantList> scanner, contentScanner;
    QVariantList contentResults;
    QMap<QString, QString> openBuffers;
    QString contentQuery, contentRoot;
    bool contentPending = false;
    void findContent();
    QFileSystemWatcher watcher, libraryWatcher;
    QProcess modules;
    QTimer debounce, refreshTimer, libraryTimer;
    bool rescan = false;
    void publish();
    void loadLibraries();
};
