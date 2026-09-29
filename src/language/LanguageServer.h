#pragma once
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QTimer>
class LanguageServer : public QObject {
    Q_OBJECT
  public:
    explicit LanguageServer(QObject *p = nullptr);
    ~LanguageServer();
    void start(const QString &node, const QString &script, const QString &root, const QString &python);
    void stop();
    void fileChanged(const QString &path);
    void configure(const QStringList &paths);
    void send(const QJsonObject &object);
    void relay(const QString &message);
    bool ready = false;
  signals:
    void message(const QString &json);
    void initialized();
    void error(const QString &text);

  private:
    QProcess process;
    QByteArray buffer;
    QString root, python;
    QStringList extraPaths;
    void read();
    void handle(const QJsonObject &obj);
};
