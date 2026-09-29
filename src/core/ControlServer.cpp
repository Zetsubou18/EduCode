#include "ControlServer.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QTimer>
#include <QUuid>
#include <memory>
ControlServer::ControlServer(QObject *parent) : QObject(parent) {
    token = QUuid::createUuid().toString(QUuid::WithoutBraces) +
            QUuid::createUuid().toString(QUuid::WithoutBraces);
    endpointPath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
                       .filePath(QString("control-%1.json").arg(QCoreApplication::applicationPid()));
    if (!server.listen(QHostAddress::LocalHost, 0))
        return;
    QSaveFile endpoint(endpointPath);
    if (endpoint.open(QIODevice::WriteOnly)) {
        endpoint.write(
            QJsonDocument(QJsonObject{{"version", 1}, {"port", int(server.serverPort())}, {"token", token}})
                .toJson());
        endpoint.commit();
        QFile::setPermissions(endpointPath, QFile::ReadOwner | QFile::WriteOwner);
    }
    connect(&server, &QTcpServer::newConnection, this, [this] {
        while (auto socket = server.nextPendingConnection()) {
            auto buffer = std::make_shared<QByteArray>();
            QTimer::singleShot(5000, socket, &QTcpSocket::abort);
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer] {
                *buffer += socket->readAll();
                if (buffer->size() > 65536) {
                    socket->abort();
                    return;
                }
                if (!buffer->contains('\n'))
                    return;
                const auto request =
                    QJsonDocument::fromJson(buffer->left(buffer->indexOf('\n'))).object().toVariantMap();
                QVariantMap response;
                if (request["token"].toString() != token)
                    response = {{"ok", false}, {"error", "Authentication failed"}};
                else
                    response =
                        dispatch ? dispatch(request) : QVariantMap{{"ok", false}, {"error", "Not ready"}};
                socket->write(QJsonDocument::fromVariant(response).toJson(QJsonDocument::Compact) + "\n");
                socket->disconnectFromHost();
            });
        }
    });
}
ControlServer::~ControlServer() {
    server.close();
    QFile::remove(endpointPath);
}
