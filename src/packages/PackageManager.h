#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
class PackageManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
    Q_PROPERTY(QVariantMap details READ details NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
  public:
    explicit PackageManager(QObject *parent = nullptr);
    ~PackageManager() override;
    QVariantList items() const {
        return packageItems;
    }
    QVariantMap details() const {
        return packageDetails;
    }
    bool busy() const {
        return process.state() != QProcess::NotRunning;
    }
    QString status() const {
        return currentStatus;
    }
    void configure(const QString &python, const QString &helper);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void select(const QString &name);
    Q_INVOKABLE void install(const QString &name);
    Q_INVOKABLE void installRequirements(const QString &path);
    Q_INVOKABLE void update(const QString &name);
    Q_INVOKABLE void remove(const QString &name);
  signals:
    void changed();
    void error(const QString &message);
    void packagesChanged();

  private:
    QString python, helper, operation, selected, currentStatus;
    QVariantList packageItems, installed;
    QVariantMap packageDetails;
    QProcess process;
    QTimer searchTimer;
    QString pendingQuery;
    void start(const QString &action, const QString &argument = QString());
};
