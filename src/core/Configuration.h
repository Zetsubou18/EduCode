#pragma once
#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantMap>
class Configuration : public QObject {
    Q_OBJECT
  public:
    explicit Configuration(QObject *parent = nullptr, const QString &filePath = QString());
    QVariantMap values;
    QString path;
    bool set(const QString &key, const QVariant &value, QString *error = nullptr);
    bool patch(const QVariantMap &changes, QString *error = nullptr);
    void reload();
    static QVariantMap defaults();
  signals:
    void changed();
    void rejected(const QString &message);

  private:
    QFileSystemWatcher watcher;
    QTimer debounce;
    bool validate(const QVariantMap &candidate, QString *error) const;
    bool write(const QVariantMap &candidate, QString *error);
};
