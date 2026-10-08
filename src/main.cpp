#include "core/AppController.h"
#include "core/Log.h"
#include "platform/WindowChrome.h"
#include "platform/DemoCapture.h"
#include <QBuffer>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QGuiApplication>
#include <QIcon>
#include <QInputMethodEvent>
#include <QMouseEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTextStream>
#include <QtWebEngine/QtWebEngine>
int main(int argc, char **argv) {
    QStringList paths;
    bool options = true, stdinDocument = false, showLogs = false, showConfig = false;
    int startLine = 0, startColumn = 0;
    for (int i = 1; i < argc; ++i) {
        const auto arg = QString::fromLocal8Bit(argv[i]);
        if (options && arg == "--") { options = false; continue; }
        if (options && (arg == "--help" || arg == "-h")) {
            QTextStream(stdout) << "Usage: EduCode [options] [directory | file ...]\n"
                << "Relative paths use the current directory.\n"
                << "  --help, -h       Show help\n  --version, -v    Show version\n"
                << "  --line N         Open at line N\n  --column N       Open at column N\n"
                << "  --stdin, -       Open piped UTF-8 text (up to 8 MiB)\n"
                << "  --logs           Print log directory\n  --config         Print config path\n"
                << "  --verbose        Also print Qt diagnostics to stderr\n  --wait           Wait until IDE closes (default)\n";
            return 0;
        }
        if (options && (arg == "--version" || arg == "-v")) {
            QTextStream(stdout) << "EduCode 0.3.0\n"; return 0;
        }
        if (options && arg == "--verbose") { qputenv("EDUCODE_VERBOSE", "1"); continue; }
        if (options && arg == "--wait") continue;
        if (options && arg == "--logs") { showLogs = true; continue; }
        if (options && arg == "--config") { showConfig = true; continue; }
        if (options && (arg == "--stdin" || arg == "-")) { stdinDocument = true; continue; }
        if (options && (arg == "--line" || arg == "--column")) {
            if (++i >= argc) { QTextStream(stderr) << "Missing value for " << arg << "\n"; return 2; }
            const auto value = QString::fromLocal8Bit(argv[i]);
            bool valid;
            const int number = value.toInt(&valid);
            if (!valid || number < 1) { QTextStream(stderr) << "Expected positive number for " << arg << "\n"; return 2; }
            if (arg == "--line") startLine = number; else startColumn = number;
            continue;
        }
        if (options && arg.startsWith('-')) { QTextStream(stderr) << "Unknown option: " << arg << "\n"; return 2; }
        if (!QFileInfo::exists(arg)) { QTextStream(stderr) << "Path does not exist: " << arg << "\n"; return 2; }
        paths << QFileInfo(arg).absoluteFilePath();
    }
    if (showLogs || showConfig) {
        QCoreApplication cli(argc, argv);
        cli.setOrganizationName("EduCode"); cli.setApplicationName("EduCode");
        QTextStream(stdout) << (showLogs ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/logs"
                                        : QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/config.json") << "\n";
        return 0;
    }
    QTemporaryDir pipedDirectory;
    if (stdinDocument) {
        QFile input; input.open(stdin, QIODevice::ReadOnly);
        const auto bytes = input.read(8 * 1024 * 1024 + 1);
        if (!pipedDirectory.isValid() || bytes.size() > 8 * 1024 * 1024 || bytes.contains(char(0))) {
            QTextStream(stderr) << "Invalid input or input exceeds 8 MiB\n"; return 2;
        }
        QFile file(pipedDirectory.filePath("stdin.txt"));
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) return 2;
        file.close(); paths << file.fileName();
    }
    Log::captureStartup();
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QtWebEngine::initialize();
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setOrganizationName(qEnvironmentVariableIsSet("EDUCODE_TEST_MODE") ? "EduCodeTests" : "EduCode");
    app.setApplicationName("EduCode");
    app.setApplicationVersion("0.3.0");
    Log::initialize();
    const QString base = QCoreApplication::applicationDirPath();
    app.setWindowIcon(QIcon(base + "/assets/app_logo.png"));
    QQuickStyle::setStyle("Default");
    AppController controller;
    controller.setObjectName("backend");
    QQmlApplicationEngine engine;
    // Prefer the single coherent Qt runtime deployed beside the executable.
    engine.addImportPath(base);
    engine.rootContext()->setContextProperty("backend", &controller);
    engine.rootContext()->setContextProperty("appBase", QUrl::fromLocalFile(base + "/").toString());
    QObject::connect(&controller, &AppController::quitApproved, &app, &QCoreApplication::quit);
    engine.load(QUrl::fromLocalFile(base + "/qml/Main.qml"));
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "EduCode: failed to load interface. Log:" << Log::path();
        return 1;
    }
    Platform::applyWindowChrome(qobject_cast<QQuickWindow *>(engine.rootObjects().first()));
    if (qEnvironmentVariableIsSet("EDUCODE_TEST_MODE")) {
        auto testWindow = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QObject::connect(
            &controller, &AppController::editorCommand, &app, [testWindow](const QString &command) {
                if (command == "test-ui-inspect") {
                    auto ide = testWindow->findChild<QObject *>("idePage");
                    if (ide)
                        qInfo() << "UI:" << ide->property("explorerOpen") << ide->property("explorerExtent");
                }
                const auto parts = command.split(' ');
                auto mouse = [testWindow](QEvent::Type type, QPointF point, Qt::MouseButton button,
                                          Qt::MouseButtons buttons) {
                    QMouseEvent event(type, point, testWindow->mapToGlobal(point.toPoint()), button, buttons,
                                      Qt::NoModifier);
                    static ulong timestamp = 0;
                    event.setTimestamp(timestamp += 20);
                    QCoreApplication::sendEvent(testWindow, &event);
                };
                if (command.startsWith("test-ui-text ")) {
                    QInputMethodEvent event;
                    event.setCommitString(command.mid(13));
                    QCoreApplication::sendEvent(QGuiApplication::focusObject(), &event);
                }
                if (parts.value(0) == "test-ui-click" && parts.size() == 4) {
                    QPointF point(parts[1].toDouble(), parts[2].toDouble());
                    auto button = parts[3] == "right" ? Qt::RightButton : Qt::LeftButton;
                    mouse(QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
                    mouse(QEvent::MouseButtonPress, point, button, button);
                    mouse(QEvent::MouseButtonRelease, point, button, Qt::NoButton);
                } else if (parts.value(0) == "test-ui-drag" && parts.size() == 5) {
                    QPointF start(parts[1].toDouble(), parts[2].toDouble()),
                        end(parts[3].toDouble(), parts[4].toDouble());
                    mouse(QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
                    for (int step = 1; step <= 10; ++step)
                        mouse(QEvent::MouseMove, start + (end - start) * step / 10, Qt::NoButton,
                              Qt::LeftButton);
                    mouse(QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
                } else if (command.startsWith("test-demo-shot ")) {
                    auto demo = testWindow->findChild<QQuickWindow *>("teacherDemo");
                    if (demo && demo->isVisible()) demo->grabWindow().save(command.mid(15));
                    else for (auto window : QGuiApplication::topLevelWindows()) {
                        auto quick = qobject_cast<QQuickWindow *>(window);
                        if (quick && quick->objectName() == "teacherDemo" && quick->isVisible()) quick->grabWindow().save(command.mid(15));
                    }
                } else if (command.startsWith("test-ui-shot "))
                    testWindow->grabWindow().save(command.mid(13));
            });
    }
    auto ideWindow = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QObject::connect(controller.teacher(), &TeacherSession::changed, &app, [&controller] { if(controller.teacher()->property("role").toString() != "teacher") DemoCapture::reset(); });
    controller.teacher()->capture = [ideWindow, &controller] {
        auto image = DemoCapture::capture(ideWindow, controller.demoProcessId());
        if (image.isNull()) return QString();
        QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG", 1);
        return QString("data:image/png;base64,") + QString::fromLatin1(bytes.toBase64());
    };
    controller.openPaths(paths, startLine, startColumn);
    if (qEnvironmentVariableIsSet("EDUCODE_SCREENSHOT")) {
        // Hidden test-process startup flags suppress the first native ShowWindow call.
        // Re-expose the window so WebEngine receives a real viewport for visual QA.
        auto testWindow = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QTimer::singleShot(100, &app, [testWindow] {
            if (testWindow) {
                testWindow->hide();
                testWindow->show();
            }
        });
        QTimer::singleShot(qEnvironmentVariableIntValue("EDUCODE_SCREENSHOT_DELAY")
                               ? qEnvironmentVariableIntValue("EDUCODE_SCREENSHOT_DELAY")
                               : 3500,
                           &app, [&] {
                               auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
                               if (window)
                                   window->grabWindow().save(qEnvironmentVariable("EDUCODE_SCREENSHOT"));
                               app.quit();
                           });
    }
    const int result = app.exec();
    Log::write("LIFECYCLE", "EduCode stopped");
    return result;
}
