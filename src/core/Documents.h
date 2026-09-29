#pragma once
#include <QMap>
#include <QObject>
#include <QVariantList>
struct Document {
    QString path, text, saved;
    bool bom = false;
    QString newline = "\n";
};
class Documents : public QObject {
    Q_OBJECT
  public:
    explicit Documents(QObject *parent = nullptr) : QObject(parent) {}
    QMap<QString, Document> entries;
    QStringList order;
    QString active;
    bool open(const QString &path, QString *error = nullptr);
    bool save(const QString &path, QString *error = nullptr);
    bool saveAll(QString *error = nullptr);
    void edit(const QString &path, const QString &text);
    void close(const QString &path);
    bool dirty(const QString &path) const;
    bool anyDirty() const;
    QVariantList tabs() const;
  signals:
    void changed();
};
