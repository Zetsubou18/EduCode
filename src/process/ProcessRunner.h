#pragma once
#include <QObject>
#include <QProcess>
#include <QTextCodec>
#include <QTimer>
#include <memory>
class ProcessRunner : public QObject {
    Q_OBJECT
  public:
    explicit ProcessRunner(QObject *parent = nullptr);
    ~ProcessRunner();
    bool demoMode = false;
    qint64 processId() const { return process.processId(); }
    bool running() const {
        return process.state() != QProcess::NotRunning;
    }
    void run(const QString &python, const QString &file, const QString &directory,
             bool systemConsole = false);
    void input(const QString &text);
    void stop();
  signals:
    void output(const QString &text);
    void runningChanged();

  private:
    bool systemOutput = false;
    QProcess process;
    QTimer timer;
    QString pending;
    std::unique_ptr<QTextDecoder> decoder;
    void drain();
    void flush();
};
