#pragma once
#include <QAbstractListModel>
#include <QJsonObject>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QVariantList>
#include <functional>
class TeacherStudentsModel : public QAbstractListModel {
    Q_OBJECT
  public:
    struct Student {
        QString id, name;
        int ping = 0;
    };
    enum Roles { StudentId = Qt::UserRole + 1, StudentName, StudentPing };
    explicit TeacherStudentsModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void add(const QString &id, const QString &name);
    void updatePing(const QString &id, int ping);
    void remove(const QString &id);
    void clear();

  private:
    QList<Student> entries;
};
class TeacherSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString role MEMBER role NOTIFY changed)
    Q_PROPERTY(QString status MEMBER status NOTIFY changed)
    Q_PROPERTY(int port MEMBER port NOTIFY changed)
    Q_PROPERTY(QVariantList students READ students NOTIFY changed)
    Q_PROPERTY(QObject *roster READ roster CONSTANT)
    Q_PROPERTY(QString remotePath MEMBER remotePath NOTIFY remoteChanged)
    Q_PROPERTY(QString remoteText MEMBER remoteText NOTIFY remoteChanged)
    Q_PROPERTY(QString frame MEMBER frame NOTIFY frameChanged)
    Q_PROPERTY(QString selected MEMBER selected NOTIFY remoteChanged)
  public:
    explicit TeacherSession(QObject *parent = nullptr);
    std::function<QJsonObject()> document;
    std::function<void(const QString &, const QString &)> applyEdit;
    std::function<QString()> displayName;
    std::function<QString()> capture;
    Q_INVOKABLE void create();
    Q_INVOKABLE void join(const QString &ip, int port);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void kick(const QString &id);
    Q_INVOKABLE void watch(const QString &id);
    Q_INVOKABLE void editRemote(const QString &text);
    QVariantList students() const;
    QObject *roster() {
        return &rosterModel;
    }
  signals:
    void changed();
    void remoteChanged();
    void frameChanged();
    void uiCommand(const QString &name);

  private:
    struct Peer {
        QString id, name, path, text;
        QByteArray buffer;
        qint64 seen = 0, sent = 0;
        int ping = 0, revision = 0;
        bool welcomed = false;
    };
    QTcpServer server;
    QTcpSocket *upstream = nullptr;
    QMap<QTcpSocket *, Peer> peers;
    TeacherStudentsModel rosterModel{this};
    QTimer timer;
    QString role = "idle", status, selected, remotePath, remoteText, frame, lastFrame;
    int port = 0, revision = 0, remoteRevision = 0;
    QJsonObject last;
    void attach(QTcpSocket *socket);
    void receive(QTcpSocket *socket, const QJsonObject &message);
    void send(QTcpSocket *socket, const QJsonObject &message);
    void tick();
    void publish(bool force = false);
};
