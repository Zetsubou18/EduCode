#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
class TerminalSession : public QObject {
    Q_OBJECT
  public:
    explicit TerminalSession(QObject *p = nullptr);
    ~TerminalSession();
    void start(const QString &node, const QString &script, const QString &root, const QString &venv);
    void input(const QString &text);
    void resize(int columns, int rows);
    void stop();
  signals:
    void output(const QString &text);
    void error(const QString &text);

  private:
    QProcess process;
    QByteArray buffer;
    QString pending;
    QTimer timer;
    void send(const QByteArray &json);
};
