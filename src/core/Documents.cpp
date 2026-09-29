#include "Documents.h"
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
bool Documents::open(const QString &path, QString *error) {
    QString p = QFileInfo(path).canonicalFilePath();
    if (p.isEmpty())
        p = QFileInfo(path).absoluteFilePath();
#ifdef Q_OS_WIN
    if (p.size() > 1 && p[1] == ':')
        p[0] = p[0].toUpper();
#endif
    if (!entries.contains(p)) {
        QFile f(p);
        if (!f.open(QIODevice::ReadOnly)) {
            if (error)
                *error = f.errorString();
            return false;
        }
        if (f.size() > 8 * 1024 * 1024) {
            if (error)
                *error = QStringLiteral("Файл больше 8 МБ. Откройте его во внешнем редакторе.");
            return false;
        }
        QByteArray bytes = f.readAll();
        Document d;
        d.path = p;
        d.bom = bytes.startsWith("\xEF\xBB\xBF");
        if (d.bom)
            bytes.remove(0, 3);
        d.text = QString::fromUtf8(bytes);
        if (d.text.contains(QChar::ReplacementCharacter)) {
            if (error)
                *error = QStringLiteral("Поддерживаются текстовые файлы UTF-8.");
            return false;
        }
        d.newline = d.text.contains("\r\n") ? QStringLiteral("\r\n") : QStringLiteral("\n");
        d.text.replace("\r\n", "\n");
        d.saved = d.text;
        entries.insert(p, d);
        order.append(p);
    }
    active = p;
    emit changed();
    return true;
}
bool Documents::save(const QString &path, QString *error) {
    if (!entries.contains(path))
        return false;
    auto &d = entries[path];
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    QString text = d.text;
    if (d.newline == "\r\n")
        text.replace("\n", "\r\n");
    QByteArray bytes = text.toUtf8();
    if (d.bom)
        bytes.prepend("\xEF\xBB\xBF");
    if (f.write(bytes) != bytes.size() || !f.commit()) {
        if (error)
            *error = f.errorString();
        return false;
    }
    d.saved = d.text;
    emit changed();
    return true;
}
bool Documents::saveAll(QString *error) {
    for (const auto &p : order)
        if (dirty(p) && !save(p, error))
            return false;
    return true;
}
void Documents::edit(const QString &p, const QString &text) {
    if (!entries.contains(p))
        return;
    bool was = dirty(p);
    entries[p].text = text;
    if (was != dirty(p))
        emit changed();
}
void Documents::close(const QString &p) {
    int i = order.indexOf(p);
    entries.remove(p);
    order.removeAll(p);
    if (active == p)
        active = order.isEmpty() ? QString() : order[qBound(0, i, order.size() - 1)];
    emit changed();
}
bool Documents::dirty(const QString &p) const {
    return entries.contains(p) && entries[p].text != entries[p].saved;
}
bool Documents::anyDirty() const {
    for (const auto &p : order)
        if (dirty(p))
            return true;
    return false;
}
QVariantList Documents::tabs() const {
    QVariantList v;
    for (const auto &p : order)
        v.append(QVariantMap{{"path", p}, {"name", QFileInfo(p).fileName()}, {"modified", dirty(p)}});
    return v;
}
