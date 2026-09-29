#include "TeacherSession.h"
#include <QDateTime>
#include <QHostAddress>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QUuid>
#include <QtEndian>
namespace {
constexpr int limit = 24 * 1024 * 1024;
qint64 now() {
    return QDateTime::currentMSecsSinceEpoch();
}
} // namespace
int TeacherStudentsModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : entries.size();
}
QVariant TeacherStudentsModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= entries.size())
        return {};
    const auto &student = entries[index.row()];
    if (role == StudentId)
        return student.id;
    if (role == StudentName)
        return student.name;
    if (role == StudentPing)
        return student.ping;
    return {};
}
QHash<int, QByteArray> TeacherStudentsModel::roleNames() const {
    return {{StudentId, "studentId"}, {StudentName, "studentName"}, {StudentPing, "studentPing"}};
}
void TeacherStudentsModel::add(const QString &id, const QString &name) {
    int row = 0;
    while (row < entries.size() && QString::localeAwareCompare(entries[row].name, name) < 0)
        ++row;
    beginInsertRows({}, row, row);
    entries.insert(row, {id, name, 0});
    endInsertRows();
}
void TeacherStudentsModel::updatePing(const QString &id, int ping) {
    for (int row = 0; row < entries.size(); ++row)
        if (entries[row].id == id && entries[row].ping != ping) {
            entries[row].ping = ping;
            emit dataChanged(index(row), index(row), {StudentPing});
            return;
        }
}
void TeacherStudentsModel::remove(const QString &id) {
    for (int row = 0; row < entries.size(); ++row)
        if (entries[row].id == id) {
            beginRemoveRows({}, row, row);
            entries.removeAt(row);
            endRemoveRows();
            return;
        }
}
void TeacherStudentsModel::clear() {
    if (entries.isEmpty())
        return;
    beginResetModel();
    entries.clear();
    endResetModel();
}
TeacherSession::TeacherSession(QObject *p) : QObject(p) {
    server.setProxy(QNetworkProxy::NoProxy);
    connect(&server, &QTcpServer::newConnection, this, [this] {
        while (server.hasPendingConnections()) {
            auto s = server.nextPendingConnection();
            if (peers.size() >= 64) {
                s->disconnectFromHost();
                s->deleteLater();
            } else
                attach(s);
        }
    });
    timer.setInterval(200);
    connect(&timer, &QTimer::timeout, this, &TeacherSession::tick);
    timer.start();
}
void TeacherSession::send(QTcpSocket *s, const QJsonObject &m) {
    if (!s || s->state() != QAbstractSocket::ConnectedState)
        return;
    auto bytes = QJsonDocument(m).toJson(QJsonDocument::Compact);
    if (bytes.size() > limit)
        return;
    if (s->bytesToWrite() > 4 * 1024 * 1024) {
        s->abort();
        return;
    }
    quint32 length = qToBigEndian(quint32(bytes.size()));
    s->write(reinterpret_cast<const char *>(&length), 4);
    s->write(bytes);
}
void TeacherSession::attach(QTcpSocket *s) {
    Peer p;
    p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    p.seen = now();
    peers.insert(s, p);
    connect(s, &QTcpSocket::readyRead, this, [this, s] {
        if (!peers.contains(s))
            return;
        auto &p = peers[s];
        p.buffer += s->readAll();
        while (p.buffer.size() >= 4) {
            quint32 n = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(p.buffer.constData()));
            if (n > limit || !n) {
                s->abort();
                return;
            }
            if (p.buffer.size() < int(n) + 4)
                break;
            QJsonParseError error;
            auto d = QJsonDocument::fromJson(p.buffer.mid(4, n), &error);
            p.buffer.remove(0, int(n) + 4);
            if (error.error != QJsonParseError::NoError || !d.isObject()) {
                s->abort();
                return;
            }
            p.seen = now();
            receive(s, d.object());
            if (!peers.contains(s))
                return;
        }
    });
    connect(s, &QTcpSocket::disconnected, this, [this, s] {
        bool client = s == upstream;
        bool watched = peers.value(s).id == selected;
        rosterModel.remove(peers.value(s).id);
        peers.remove(s);
        if (client) {
            upstream = nullptr;
            role = "idle";
            port = 0;
            remotePath.clear();
            remoteText.clear();
            frame.clear();
            lastFrame.clear();
            if (status == QStringLiteral("Подключено") || status == QStringLiteral("Подключение…") ||
                status.isEmpty())
                status = QStringLiteral("Отключено");
            emit remoteChanged();
            emit frameChanged();
        } else if (watched) {
            selected.clear();
            remotePath.clear();
            remoteText.clear();
            emit remoteChanged();
        }
        s->deleteLater();
        emit changed();
    });
    connect(s, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this,
            [this, s](QAbstractSocket::SocketError) {
                const auto message = s->errorString();
                if (s == upstream && s->state() == QAbstractSocket::UnconnectedState)
                    stop();
                status = message;
                emit changed();
            });
}
void TeacherSession::create() {
    stop();
    status.clear();
    if (!server.listen(QHostAddress::AnyIPv4, 0)) {
        status = server.errorString();
        emit changed();
        return;
    }
    role = "teacher";
    port = server.serverPort();
    last = {};
    revision = 0;
    status = QStringLiteral("Ожидание учеников");
    emit changed();
}
void TeacherSession::join(const QString &ip, int number) {
    QHostAddress address;
    if (!address.setAddress(ip.trimmed()) || number < 1 || number > 65535) {
        status = QStringLiteral("Введите IP и порт 1–65535");
        emit changed();
        return;
    }
    stop();
    status = QStringLiteral("Подключение…");
    role = "connecting";
    auto s = new QTcpSocket(this);
    s->setProxy(QNetworkProxy::NoProxy);
    upstream = s;
    attach(s);
    connect(s, &QTcpSocket::connected, this, [this, s] {
        send(s, {{"type", "hello"},
                 {"version", 1},
                 {"name", displayName ? displayName() : QStringLiteral("Ученик")}});
    });
    s->connectToHost(address, quint16(number));
    emit changed();
}
void TeacherSession::stop() {
    status.clear();
    server.close();
    auto sockets = peers.keys();
    upstream = nullptr;
    for (auto s : sockets) {
        s->disconnect(this);
        s->abort();
        s->deleteLater();
    }
    peers.clear();
    rosterModel.clear();
    role = "idle";
    port = 0;
    selected.clear();
    remotePath.clear();
    remoteText.clear();
    frame.clear();
    lastFrame.clear();
    last = {};
    emit changed();
    emit remoteChanged();
    emit frameChanged();
}
QVariantList TeacherSession::students() const {
    QVariantList result;
    for (const auto &p : peers)
        if (p.welcomed)
            result.append(QVariantMap{{"id", p.id}, {"name", p.name}, {"ping", p.ping}});
    return result;
}
void TeacherSession::kick(const QString &id) {
    if (role != "teacher")
        return;
    for (auto s : peers.keys())
        if (peers[s].id == id) {
            send(s, {{"type", "end"}, {"reason", QStringLiteral("Учитель отключил вас")}});
            s->disconnectFromHost();
        }
}
void TeacherSession::watch(const QString &id) {
    if (role != "teacher")
        return;
    for (const auto &p : peers)
        if (p.id == id && p.welcomed) {
            selected = id;
            remotePath = p.path;
            remoteText = p.text;
            remoteRevision = p.revision;
            emit remoteChanged();
        }
}
void TeacherSession::editRemote(const QString &text) {
    if (role != "teacher" || text.size() > 8 * 1024 * 1024)
        return;
    for (auto s : peers.keys())
        if (peers[s].id == selected)
            send(s, {{"type", "edit"}, {"path", remotePath}, {"text", text}, {"base", remoteRevision}});
}
void TeacherSession::receive(QTcpSocket *s, const QJsonObject &m) {
    auto &p = peers[s];
    QString type = m["type"].toString();
    if (role == "teacher" && type == "hello" && !p.welcomed) {
        if (m["version"].toInt() != 1) {
            s->abort();
            return;
        }
        p.name = m["name"].toString().trimmed().left(80);
        if (p.name.isEmpty())
            p.name = QStringLiteral("Ученик");
        p.welcomed = true;
        rosterModel.add(p.id, p.name);
        status = QStringLiteral("Конференция активна");
        send(s, {{"type", "welcome"}});
        publish(true);
        if (!lastFrame.isEmpty())
            send(s, {{"type", "frame"}, {"data", lastFrame}});
        emit changed();
        return;
    }
    if (s == upstream && type == "welcome" && role == "connecting") {
        role = "student";
        status = QStringLiteral("Подключено");
        p.welcomed = true;
        last = {};
        publish(true);
        emit changed();
        return;
    }
    if (!p.welcomed)
        return;
    if (type == "ping") {
        send(s, {{"type", "pong"}, {"time", m["time"]}});
        return;
    }
    if (type == "pong") {
        p.ping = int(qBound<qint64>(0, now() - m["time"].toVariant().toLongLong(), 60000));
        rosterModel.updatePing(p.id, p.ping);
        emit changed();
        return;
    }
    if (type == "code") {
        QString path = m["path"].toString(), text = m["text"].toString();
        if (path.size() > 4096 || text.toUtf8().size() > 8 * 1024 * 1024) {
            s->abort();
            return;
        }
        p.path = path;
        p.text = text;
        p.revision = m["revision"].toInt();
        if (s == upstream || p.id == selected) {
            remotePath = path;
            remoteText = text;
            remoteRevision = p.revision;
            emit remoteChanged();
        }
        return;
    }
    if (s == upstream && type == "frame") {
        auto data = m["data"].toString();
        if (data.size() <= 12 * 1024 * 1024 && data != frame) {
            frame = data;
            emit frameChanged();
        }
        return;
    }
    if (s == upstream && type == "edit" && role == "student") {
        publish();
        auto d = document ? document() : QJsonObject{};
        if (m["base"].toInt(-1) == revision && m["path"] == d["path"] &&
            m["text"].toString().toUtf8().size() <= 8 * 1024 * 1024 && applyEdit)
            applyEdit(d["path"].toString(), m["text"].toString());
        else
            send(s, {{"type", "editRejected"}});
        publish(true);
        return;
    }
    if (role == "teacher" && type == "editRejected") {
        status = QStringLiteral("Ученик успел изменить код. Получена новая версия — повторите правку.");
        emit changed();
        return;
    }
    if (s == upstream && type == "end") {
        status = m["reason"].toString();
        s->disconnectFromHost();
    }
}
void TeacherSession::publish(bool force) {
    if (role != "teacher" && role != "student")
        return;
    auto d = document ? document() : QJsonObject{};
    if (d != last) {
        last = d;
        ++revision;
    } else if (!force)
        return;
    d["type"] = "code";
    d["revision"] = revision;
    for (auto s : peers.keys())
        if (peers[s].welcomed)
            send(s, d);
}
void TeacherSession::tick() {
    const auto time = now();
    for (auto s : peers.keys()) {
        auto &p = peers[s];
        if (time - p.seen > 15000) {
            s->abort();
            continue;
        }
        if (p.welcomed && time - p.sent >= 2000) {
            p.sent = time;
            send(s, {{"type", "ping"}, {"time", time}});
        }
    }
    publish();
    if (role == "teacher" && !peers.isEmpty() && capture) {
        auto data = capture();
        if (!data.isEmpty() && data != lastFrame) {
            lastFrame = data;
            for (auto s : peers.keys())
                if (peers[s].welcomed)
                    send(s, {{"type", "frame"}, {"data", data}});
        }
    }
}
