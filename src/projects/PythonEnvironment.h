#pragma once
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QTimer>
#include <QVariantList>
class PythonEnvironment : public QObject {
    Q_OBJECT
  public:
    explicit PythonEnvironment(QObject *p = nullptr);
    QVariantList versions;
    bool busy = false;
    void discover();
    void probe(const QString &path);
    void create(const QString &project, const QString &python);
    static QString interpreter(const QString &project);
  signals:
    void changed();
    void created(const QString &project);
    void error(const QString &message);

  private:
    QProcess creator;
    QString creatingProject;
    QStringList candidates;
    QSet<QString> seen;
    void next();
};
