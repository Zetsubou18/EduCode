#pragma once
#include <QNetworkAccessManager>
#include <QObject>
#include <QProcess>
#include <QVariantList>
#include <functional>
class AiAssistant : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList chats READ chats NOTIFY changed)
    Q_PROPERTY(QVariantList messages READ messages NOTIFY changed)
    Q_PROPERTY(QString activeChat READ activeChat NOTIFY changed)
    Q_PROPERTY(QString activity READ activity NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool ollamaAvailable READ ollamaAvailable NOTIFY changed)
    Q_PROPERTY(QVariantList models READ models NOTIFY changed)
  public:
    explicit AiAssistant(QObject *parent = nullptr);
    ~AiAssistant() override;
    QVariantList chats() const;
    QVariantList messages() const;
    QString activeChat() const {
        return currentId;
    }
    QString activity() const {
        return currentActivity;
    }
    bool busy() const {
        return process.state() != QProcess::NotRunning;
    }
    bool ollamaAvailable() const {
        return available;
    }
    QVariantList models() const {
        return modelList;
    }
    void configure(const QVariantMap &settings, const QString &helper, const QString &endpoint,
                   const QString &python);
    Q_INVOKABLE void probe();
    Q_INVOKABLE void createChat();
    Q_INVOKABLE void selectChat(const QString &id);
    Q_INVOKABLE void deleteChat(const QString &id);
    Q_INVOKABLE void send(const QString &prompt);
    Q_INVOKABLE void retry();
    Q_INVOKABLE void stop();
  signals:
    void changed();
    void error(const QString &text);
    void requestInstall();

  private:
    QVariantList data, modelList;
    QString currentId, currentActivity, helper, endpoint, requestPath, runtimePython;
    QVariantMap settings;
    QProcess process;
    QByteArray buffer;
    bool available = false;
    bool retrying = false;
    QNetworkAccessManager network;
    QString statePath;
    int chatIndex() const;
    void save();
    void load();
    void handle(const QVariantMap &event);
};
