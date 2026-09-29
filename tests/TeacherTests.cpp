#include "core/TeacherSession.h"
#include <QJsonDocument>
#include <QtEndian>
#include <QtTest>
class TeacherTests : public QObject {
    Q_OBJECT
  private slots:
    void classroom() {
        QJsonObject teacherDoc{{"path", "teacher.py"}, {"text", "print('teacher')"}},
            studentDoc{{"path", "student.py"}, {"text", "print('student')"}};
        TeacherSession host, student, second;
        host.document = [&] { return teacherDoc; };
        student.document = [&] { return studentDoc; };
        student.displayName = [] { return QString::fromUtf8("Ученик один"); };
        second.displayName = [] { return "Two"; };
        student.applyEdit = [&](const QString &path, const QString &text) {
            QCOMPARE(path, QString("student.py"));
            studentDoc["text"] = text;
        };
        host.capture = [] { return "data:image/jpeg;base64,test"; };
        host.create();
        QCOMPARE(host.property("role").toString(), QString("teacher"));
        QVERIFY(host.property("port").toInt() > 0);
        student.join("127.0.0.1", host.property("port").toInt());
        second.join("127.0.0.1", host.property("port").toInt());
        QTRY_COMPARE(host.students().size(), 2);
        QTRY_COMPARE(student.property("role").toString(), QString("student"));
        QTRY_COMPARE(student.property("remoteText").toString(), teacherDoc["text"].toString());
        QTRY_VERIFY(!student.property("frame").toString().isEmpty());
        QString id;
        for (auto value : host.students()) {
            auto p = value.toMap();
            if (p["name"].toString() == QString::fromUtf8("Ученик один"))
                id = p["id"].toString();
        }
        QVERIFY(!id.isEmpty());
        host.watch(id);
        QTRY_COMPARE(host.property("remoteText").toString(), studentDoc["text"].toString());
        studentDoc["text"] = "local typing";
        QTRY_COMPARE(host.property("remoteText").toString(), QString("local typing"));
        host.editRemote("teacher correction");
        QTRY_COMPARE(studentDoc["text"].toString(), QString("teacher correction"));
        QTRY_COMPARE(host.property("remoteText").toString(), QString("teacher correction"));
        // A local change between observation and correction must survive a stale edit.
        studentDoc["text"] = "newer local typing";
        host.editRemote("stale correction");
        QTRY_COMPARE(host.property("remoteText").toString(), QString("newer local typing"));
        QCOMPARE(studentDoc["text"].toString(), QString("newer local typing"));
        teacherDoc["path"] = "next.py";
        teacherDoc["text"] = "next teacher file";
        QTRY_COMPARE(student.property("remotePath").toString(), QString("next.py"));
        QTRY_COMPARE(student.property("remoteText").toString(), QString("next teacher file"));
        host.kick(id);
        QTRY_COMPARE(student.property("role").toString(), QString("idle"));
        QTRY_COMPARE(host.students().size(), 1);
        QVERIFY(host.property("selected").toString().isEmpty());
        host.stop();
        QTRY_COMPARE(second.property("role").toString(), QString("idle"));
        QCOMPARE(host.property("port").toInt(), 0);
        host.create();
        QVERIFY(host.property("port").toInt() > 0);
        host.stop();
    }
    void framingAndInput() {
        TeacherSession host;
        host.create();
        QTcpSocket peer;
        peer.connectToHost(QHostAddress::LocalHost, host.property("port").toInt());
        QVERIFY(peer.waitForConnected());
        auto bytes = QJsonDocument(QJsonObject{{"type", "hello"}, {"version", 1}, {"name", "Fragmented"}})
                         .toJson(QJsonDocument::Compact);
        quint32 size = qToBigEndian(quint32(bytes.size()));
        QByteArray packet(reinterpret_cast<char *>(&size), 4);
        packet += bytes;
        peer.write(packet.left(2));
        peer.flush();
        QTest::qWait(50);
        QCOMPARE(host.students().size(), 0);
        peer.write(packet.mid(2));
        peer.flush();
        QTRY_COMPARE(host.students().size(), 1);
        quint32 oversized = qToBigEndian(quint32(25 * 1024 * 1024));
        peer.write(reinterpret_cast<char *>(&oversized), 4);
        peer.flush();
        QTRY_COMPARE(host.students().size(), 0);
        TeacherSession invalid;
        invalid.join("", 0);
        QCOMPARE(invalid.property("role").toString(), QString("idle"));
        QVERIFY(!invalid.property("status").toString().isEmpty());
        host.stop();
    }
    void unchangedFramesStayStable() {
        TeacherSession host, student, lateStudent;
        QString image = "data:image/png;base64,first";
        host.capture = [&] { return image; };
        host.create();
        student.join("127.0.0.1", host.property("port").toInt());
        QTRY_COMPARE(student.property("frame").toString(), image);
        QSignalSpy frames(&student, &TeacherSession::frameChanged);
        QTest::qWait(650);
        QCOMPARE(frames.size(), 0);
        lateStudent.join("127.0.0.1", host.property("port").toInt());
        QTRY_COMPARE(lateStudent.property("frame").toString(), image);
        image = "data:image/png;base64,second";
        QTRY_COMPARE(student.property("frame").toString(), image);
        QTRY_COMPARE(lateStudent.property("frame").toString(), image);
        QCOMPARE(frames.size(), 1);
        host.stop();
    }
    void rosterUpdatesWithoutReset() {
        TeacherStudentsModel roster;
        QSignalSpy resets(&roster, &QAbstractItemModel::modelReset);
        QSignalSpy pingChanges(&roster, &QAbstractItemModel::dataChanged);
        roster.add("b", "Борис");
        roster.add("a", "Анна");
        QCOMPARE(roster.rowCount(), 2);
        QCOMPARE(roster.data(roster.index(0), TeacherStudentsModel::StudentName).toString(),
                 QStringLiteral("Анна"));
        roster.updatePing("a", 42);
        QCOMPARE(roster.data(roster.index(0), TeacherStudentsModel::StudentPing).toInt(), 42);
        QCOMPARE(pingChanges.size(), 1);
        QCOMPARE(resets.size(), 0);
        roster.remove("b");
        QCOMPARE(roster.rowCount(), 1);
    }
};
QTEST_GUILESS_MAIN(TeacherTests)
#include "TeacherTests.moc"
