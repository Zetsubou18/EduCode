#include "core/Configuration.h"
#include "core/Documents.h"
#include "process/ProcessRunner.h"
#include "projects/FileSystem.h"
#include "projects/PythonEnvironment.h"
#include "search/SearchService.h"
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
class CoreTests : public QObject {
    Q_OBJECT
  private slots:
    void configurationValidationAndReload() {
        QTemporaryDir temp;
        Configuration config(nullptr, temp.filePath("config.json"));
        QString error;
        QVERIFY(config.set("editor.fontSize", 18, &error));
        QVERIFY(!config.set("editor.fontSize", 200, &error));
        QCOMPARE(config.values["editor.fontSize"].toInt(), 18);
        QVERIFY(!config.set("browser.homePage", "file:///secret", &error));
        auto keys = config.values["hotkeys"].toMap();
        keys["run"] = "Ctrl+S";
        QVERIFY(!config.set("hotkeys", keys, &error));
        QFile file(config.path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{ broken");
        file.close();
        config.reload();
        QCOMPARE(config.values["editor.fontSize"].toInt(), 18);
        auto data = config.values;
        data["terminal.fontSize"] = 19;
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument::fromVariant(data).toJson());
        file.close();
        QTRY_COMPARE_WITH_TIMEOUT(config.values["terminal.fontSize"].toInt(), 19, 3000);
    }
    void contentSearchAndUnsavedBuffers() {
        const auto python = qEnvironmentVariable("EDUCODE_TEST_PYTHON");
        if (python.isEmpty())
            QSKIP("Set EDUCODE_TEST_PYTHON");
        QTemporaryDir temp;
        QFile file(temp.filePath("source.py"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("value = 1\nprint('needle_edu_test')\n");
        file.close();
        SearchService search;
        search.setProject(temp.path(), python);
        QTRY_VERIFY_WITH_TIMEOUT(!search.runTargets.isEmpty(), 3000);
        search.query("needle_edu_test");
        auto has = [&](int line) {
            for (const auto &entry : search.results) {
                const auto m = entry.toMap();
                if (m["line"].toInt() == line && m["path"].toString() == file.fileName())
                    return true;
            }
            return false;
        };
        QTRY_VERIFY_WITH_TIMEOUT(has(2), 5000);
        search.updateDocument(file.fileName(), "# needle_edu_test\n");
        QTRY_VERIFY_WITH_TIMEOUT(has(1), 5000);
        search.query("no_match_edu_999");
        QTRY_VERIFY_WITH_TIMEOUT(search.results.isEmpty(), 5000);
    }
    void environmentCreation() {
        QString py = qEnvironmentVariable("EDUCODE_TEST_PYTHON");
        if (py.isEmpty())
            QSKIP("Set EDUCODE_TEST_PYTHON");
        QTemporaryDir temp;
        PythonEnvironment environment;
        QSignalSpy created(&environment, &PythonEnvironment::created);
        QSignalSpy errors(&environment, &PythonEnvironment::error);
        QString initialText;
        connect(&environment, &PythonEnvironment::created, this, [&](const QString &path) {
            QFile file(QDir(path).filePath("main.py"));
            if (file.open(QIODevice::ReadOnly))
                initialText = QString::fromUtf8(file.readAll());
        });
        environment.create(temp.filePath("project"), py);
        QTRY_VERIFY_WITH_TIMEOUT(created.size() == 1 || !errors.isEmpty(), 30000);
        QVERIFY2(errors.isEmpty(), errors.isEmpty() ? "" : qPrintable(errors.first()[0].toString()));
        const QString project = temp.filePath("project");
        QVERIFY(QFileInfo::exists(PythonEnvironment::interpreter(project)));
        QVERIFY(QFileInfo::exists(QDir(project).filePath("main.py")));
        QProcess process;
        process.start(PythonEnvironment::interpreter(project), {"-c", "import sys;print(sys.prefix)"});
        QVERIFY(process.waitForFinished(5000));
        QVERIFY(QString::fromUtf8(process.readAllStandardOutput()).contains(".venv"));
        QVERIFY(initialText.contains("name = input"));
        QVERIFY(initialText.contains(QString::fromUtf8("Привет")));
        QVERIFY(!environment.busy);
    }
    void documentsPreserveEdits() {
        QTemporaryDir d;
        QString a = d.filePath("a.py"), b = d.filePath("b.py");
        QFile f(a);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("print('a')\r\n");
        f.close();
        f.setFileName(b);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("print('b')\n");
        f.close();
        Documents docs;
        QVERIFY(docs.open(a));
        docs.edit(a, "print('edited')\n");
        QVERIFY(docs.open(b));
        QVERIFY(docs.open(a));
        QCOMPARE(docs.entries[a].text, QString("print('edited')\n"));
        QVERIFY(docs.dirty(a));
        QVERIFY(docs.save(a));
        QVERIFY(!docs.dirty(a));
        f.setFileName(a);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("print('edited')\r\n"));
    }
    void filesystemBoundaries() {
        QTemporaryDir d;
        FileSystem fs;
        fs.setRoot(d.path());
        QString error;
        QVERIFY(fs.create(d.path(), "code.py", false, &error));
        QVERIFY(!fs.create(d.path(), "../escape.py", false, &error));
        QVERIFY(!fs.contains(QDir(d.path()).filePath("../outside")));
        QVERIFY(fs.rename(d.filePath("code.py"), "hello.py", &error));
        fs.clipboard = d.filePath("hello.py");
        QVERIFY(fs.create(d.path(), "sub", true, &error));
        QVERIFY(fs.paste(d.filePath("sub"), &error));
        QTRY_VERIFY(QFileInfo::exists(d.filePath("sub/hello.py")));
    }
    void runnerSupportsInput() {
        QString py = qEnvironmentVariable("EDUCODE_TEST_PYTHON");
        if (py.isEmpty())
            QSKIP("Set EDUCODE_TEST_PYTHON");
        QTemporaryDir d;
        QFile f(d.filePath("main.py"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("print('готов', flush=True)\nname=input('name: ')\nprint('Привет, '+name)\n");
        f.close();
        ProcessRunner r;
        QSignalSpy output(&r, &ProcessRunner::output);
        r.run(py, f.fileName(), d.path());
        QTRY_VERIFY_WITH_TIMEOUT(r.running(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(output.size() > 1, 5000);
        r.input(QString::fromUtf8("Мир\n"));
        QTRY_VERIFY_WITH_TIMEOUT(!r.running(), 5000);
        QString all;
        for (const auto &args : output)
            all += args[0].toString();
        QVERIFY(all.contains(QString::fromUtf8("Привет, Мир")));
    }
    void outputBenchmark() {
        QString py = qEnvironmentVariable("EDUCODE_TEST_PYTHON");
        if (py.isEmpty())
            QSKIP("Set EDUCODE_TEST_PYTHON");
        QTemporaryDir d;
        QFile f(d.filePath("output.py"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("for i in range(100000):\n    print(f'{i}: console output performance check')\n");
        f.close();
        QElapsedTimer clock;
        clock.start();
        QProcess native;
        native.setProcessChannelMode(QProcess::MergedChannels);
        native.start(py, {"-u", f.fileName()});
        QVERIFY(native.waitForFinished(15000));
        const qint64 nativeMs = clock.elapsed();
        ProcessRunner runner;
        qsizetype bytes = 0;
        int batches = 0;
        connect(&runner, &ProcessRunner::output, this, [&](const QString &s) {
            bytes += s.size();
            ++batches;
        });
        clock.restart();
        runner.run(py, f.fileName(), d.path());
        QTRY_VERIFY_WITH_TIMEOUT(runner.running(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!runner.running(), 15000);
        const qint64 ideMs = clock.elapsed();
        qInfo() << "100000 output lines: native" << nativeMs << "ms; runner" << ideMs << "ms;" << batches
                << "batches;" << bytes << "characters";
        QVERIFY(bytes > 3000000);
        QVERIFY(batches < 1000);
        QVERIFY(ideMs < nativeMs * 2 + 500);
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "CoreTests.moc"
