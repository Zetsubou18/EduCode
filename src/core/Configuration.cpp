#include "Configuration.h"
#include "Commands.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
QVariantMap Configuration::defaults() {
    return {{"schemaVersion", 1},
            {"general.saveBeforeRun", true},
            {"general.language", "ru"},
            {"general.displayName", qEnvironmentVariable("USERNAME", qEnvironmentVariable("USER", "Ученик"))},
            {"general.projectsDirectory",
             QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                 .filePath("EduCode Projects")},
            {"editor.hotkeys", QVariantMap{}},
            {"editor.fontSize", 14},
            {"editor.tabSize", 4},
            {"editor.wordWrap", false},
            {"editor.minimap", false},
            {"terminal.fontSize", 13},
            {"terminal.scrollback", 5000},
            {"console.output", "ide"},
            {"browser.homePage", "https://www.google.com"},
            {"notifications.delivery", "ide"},
            {"updates.enabled", true},
            {"python.extraPaths", QStringList{}},
            {"ai.enabled", true},
            {"ai.geminiOutputTokens", 2048},
            {"ai.provider", "gemini"},
            {"ai.geminiApiKey", ""},
            {"ai.geminiModel", "gemini-3.1-flash-lite"},
            {"ai.url", "http://127.0.0.1:11434"},
            {"ai.model", ""},
            {"ai.userPrompt", ""},
            {"ai.factMemory", QVariantMap{}},
            {"ai.chatMemoryLimit", 24000},
            {"ai.contextSize", 8192},
            {"ai.askButtons", true},
            {"hotkeys", Commands::defaultBindings()},
            {"extensions", QVariantMap{}}};
}
Configuration::Configuration(QObject *parent, const QString &filePath) : QObject(parent) {
    path = filePath.isEmpty() ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
                                    .filePath("config.json")
                              : filePath;
    QDir().mkpath(QFileInfo(path).absolutePath());
    values = defaults();
    if (QFileInfo::exists(path))
        reload();
    else {
        QString error;
        if (!write(values, &error))
            emit rejected(error);
    }
    debounce.setSingleShot(true);
    debounce.setInterval(150);
    connect(&watcher, &QFileSystemWatcher::fileChanged, this, [this] { debounce.start(); });
    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, [this] { debounce.start(); });
    connect(&debounce, &QTimer::timeout, this, &Configuration::reload);
    watcher.addPath(QFileInfo(path).absolutePath());
    watcher.addPath(path);
}
bool Configuration::validate(const QVariantMap &candidate, QString *error) const {
    auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };
    const auto base = defaults();
    for (auto it = candidate.begin(); it != candidate.end(); ++it) {
        if (!base.contains(it.key()))
            return fail("Неизвестная настройка: " + it.key());
        if (it.key() == "schemaVersion" && it.value().toInt() != 1)
            return fail("Версия конфигурации не поддерживается.");
        if (base[it.key()].type() == QVariant::Bool && it.value().type() != QVariant::Bool)
            return fail("Ожидается true или false: " + it.key());
        if (base[it.key()].type() == QVariant::Map && it.value().type() != QVariant::Map)
            return fail("Ожидается объект: " + it.key());
        if (base[it.key()].type() == QVariant::String && it.value().type() != QVariant::String)
            return fail("Ожидается строка: " + it.key());
        if (base[it.key()].type() == QVariant::Int) {
            bool ok = false;
            const auto number = it.value().toDouble(&ok);
            if (!ok || number != it.value().toInt())
                return fail("Ожидается целое число: " + it.key());
        }
    }
    for (const auto &key : QStringList{"editor.fontSize", "terminal.fontSize"})
        if (candidate[key].toInt() < 9 || candidate[key].toInt() > 32)
            return fail("Размер шрифта должен быть от 9 до 32.");
    if (candidate["editor.tabSize"].toInt() < 1 || candidate["editor.tabSize"].toInt() > 8)
        return fail("Отступ должен быть от 1 до 8.");
    if (candidate["terminal.scrollback"].toInt() < 100 || candidate["terminal.scrollback"].toInt() > 50000)
        return fail("История терминала должна быть от 100 до 50000 строк.");
    if (candidate["ai.chatMemoryLimit"].toInt() < 4000 || candidate["ai.chatMemoryLimit"].toInt() > 100000)
        return fail("Память чата должна быть от 4000 до 100000 символов.");
    if (candidate["ai.contextSize"].toInt() < 2048 || candidate["ai.contextSize"].toInt() > 131072)
        return fail("Контекст модели должен быть от 2048 до 131072 токенов.");
    if (!QStringList{"ollama", "gemini"}.contains(candidate["ai.provider"].toString()))
        return fail("Неизвестный провайдер ИИ.");
    if (candidate["ai.geminiOutputTokens"].toInt() < 128 ||
        candidate["ai.geminiOutputTokens"].toInt() > 65536)
        return fail("Лимит ответа должен быть от 128 до 65536 токенов.");
    if (candidate["general.displayName"].toString().trimmed().isEmpty() || candidate["general.displayName"].toString().size() > 80)
        return fail("Имя должно содержать от 1 до 80 символов.");
    const QUrl aiUrl(candidate["ai.url"].toString());
    if (!aiUrl.isValid() || aiUrl.host().isEmpty() || !QStringList{"https", "http"}.contains(aiUrl.scheme()))
        return fail("Адрес Ollama должен быть URL http:// или https://.");
    if (!QStringList{"ide", "system", "both"}.contains(candidate["notifications.delivery"].toString()))
        return fail("Неизвестный режим уведомлений.");
    if (!QStringList{"ide", "system"}.contains(candidate["console.output"].toString()))
        return fail("Неизвестный режим консоли.");
    const QUrl url(candidate["browser.homePage"].toString());
    if (!url.isValid() || url.host().isEmpty() || !QStringList{"https", "http"}.contains(url.scheme()))
        return fail("Начальная страница должна быть адресом http:// или https://.");
    const auto bindings = candidate["hotkeys"].toMap();
    QSet<QString> seen;
    for (auto it = bindings.begin(); it != bindings.end(); ++it) {
        if (!Commands::defaultBindings().contains(it.key()))
            return fail("Неизвестная команда: " + it.key());
        const auto sequence = it.value().toString();
        if (!sequence.isEmpty()) {
            QKeySequence key(sequence, QKeySequence::PortableText);
            if (key.isEmpty() || key.count() != 1 || (key[0] & ~Qt::KeyboardModifierMask) == Qt::Key_unknown)
                return fail("Некорректная комбинация: " + sequence);
            auto normalized = key.toString(QKeySequence::PortableText);
            if (seen.contains(normalized))
                return fail("Комбинация уже занята: " + normalized);
            seen.insert(normalized);
        }
    }
    if (bindings.size() != Commands::defaultBindings().size())
        return fail("В конфигурации должны быть все основные команды.");
    const auto overrides = candidate["editor.hotkeys"].toMap();
    for (auto it = overrides.begin(); it != overrides.end(); ++it) {
        QKeySequence key(it.value().toString(), QKeySequence::PortableText);
        if (!it.value().toString().isEmpty() &&
            (key.isEmpty() || key.count() > 2 || (key[0] & ~Qt::KeyboardModifierMask) == Qt::Key_unknown ||
             (key.count() == 2 && (key[1] & ~Qt::KeyboardModifierMask) == Qt::Key_unknown)))
            return fail("Некорректная комбинация редактора.");
    }
    if (candidate["general.projectsDirectory"].toString().trimmed().isEmpty())
        return fail("Укажите папку проектов.");
    const auto paths = candidate["python.extraPaths"];
    if (paths.type() != QVariant::List && paths.type() != QVariant::StringList)
        return fail("Пути Python должны быть списком строк.");
    for (const auto &entry : paths.toList())
        if (entry.type() != QVariant::String || entry.toString().trimmed().isEmpty())
            return fail("Каждый путь Python должен быть непустой строкой.");
    return true;
}
bool Configuration::write(const QVariantMap &candidate, QString *error) {
    QSaveFile file(path);
    const auto data = QJsonDocument::fromVariant(candidate).toJson(QJsonDocument::Indented);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error)
            *error = "Не удалось сохранить настройки: " + file.errorString();
        return false;
    }
    return true;
}
bool Configuration::set(const QString &key, const QVariant &value, QString *error) {
    return patch({{key, value}}, error);
}
bool Configuration::patch(const QVariantMap &changes, QString *error) {
    auto candidate = values;
    for (auto it = changes.begin(); it != changes.end(); ++it)
        candidate[it.key()] = it.value();
    if (!validate(candidate, error) || !write(candidate, error))
        return false;
    values = candidate;
    emit changed();
    return true;
}
void Configuration::reload() {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
    QString error;
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) {
        emit rejected("Конфигурация не загружена: некорректный JSON.");
        return;
    }
    auto candidate = defaults();
    auto data = doc.object().toVariantMap();
    data.remove("network.proxy");
    if (data.contains("ai.userMemory")) {
        auto facts = data.value("ai.factMemory").toMap();
        const auto legacyMemory = data.value("ai.userMemory").toMap();
        for (auto it = legacyMemory.begin(); it != legacyMemory.end(); ++it)
            if (!facts.contains(it.key()))
                facts.insert(it.key(), it.value());
        data["ai.factMemory"] = facts;
        data.remove("ai.userMemory");
    }
    // One-time migration from the removed Groq provider. Provider-specific keys and model ids
    // are not transferable: carrying them into Gemini causes deterministic 401/404 failures.
    if (data.value("ai.provider").toString() == "groq")
        data["ai.provider"] = "gemini";
    if (!data.contains("ai.geminiApiKey") && data.contains("ai.groqApiKey"))
        data["ai.geminiApiKey"] = "";
    if (!data.contains("ai.geminiModel") && data.contains("ai.groqModel"))
        data["ai.geminiModel"] = "gemini-3.1-flash-lite";
    if (!data.contains("ai.geminiOutputTokens") && data.contains("ai.groqOutputTokens"))
        data["ai.geminiOutputTokens"] = data.value("ai.groqOutputTokens");
    for (const auto &legacy : QStringList{"ai.groqApiKey", "ai.groqModel", "ai.groqOutputTokens",
                                          "ai.groqRPM", "ai.groqRPD", "ai.groqTPM", "ai.groqTPD"})
        data.remove(legacy);
    const auto geminiModel = data.value("ai.geminiModel").toString().trimmed().toLower();
    if (geminiModel.startsWith("openai/") || geminiModel.contains("gpt-oss") ||
        geminiModel.startsWith("llama") || geminiModel.startsWith("meta-llama/"))
        data["ai.geminiModel"] = "gemini-3.1-flash-lite";
    if (data.value("ai.geminiApiKey").toString().startsWith("gsk_"))
        data["ai.geminiApiKey"] = "";
    for (auto it = data.begin(); it != data.end(); ++it)
        candidate[it.key()] = it.value();
    auto bindings = Commands::defaultBindings();
    const auto storedBindings = candidate["hotkeys"].toMap();
    for (auto it = storedBindings.begin(); it != storedBindings.end(); ++it)
        bindings[it.key()] = it.value();
    if (bindings.value("palette").toString() == "F1") {
        bindings["palette"] = "Ctrl+Shift+P";
    }
    candidate["hotkeys"] = bindings;
    if (!validate(candidate, &error)) {
        emit rejected(error);
        return;
    }
    if (values != candidate) {
        values = candidate;
        emit changed();
    }
    if (!watcher.files().contains(path))
        watcher.addPath(path);
}
