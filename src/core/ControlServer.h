#pragma once
#include <QObject>
#include <QTcpServer>
#include <QVariantMap>
#include <functional>
class ControlServer : public QObject {
    Q_OBJECT
  public:
    explicit ControlServer(QObject *parent = nullptr);
    ~ControlServer();
    QString endpointPath;
    std::function<QVariantMap(const QVariantMap &)> dispatch;

  private:
    QTcpServer server;
    QString token;
};
