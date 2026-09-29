# Создание плагинов EduCode / Building EduCode plugins

## Русский

### 1. Что можно расширить

Плагин — обычный Python-код, который **добавляет** пункты глобального поиска, пункты контекстного меню файлов, правую HTML-панель, настройки, подсветку расширения файла и пакет перевода интерфейса. API регистрации не содержит операций удаления встроенных функций EduCode. Плагин выполняется с правами вашего пользователя, поэтому устанавливайте код только из доверенного источника. Библиотека `import educode.plugin` предназначена для *автора плагина*. `import educode` — отдельный API работающей IDE, описанный в [руководстве по терминалу](TERMINAL_API.md).

### 2. Структура и первый запуск

Создайте папку `hello-plugin` с файлами:

```text
hello-plugin/
├── plugin.json
├── plugin.py
└── panel.html
```

`plugin.json` — UTF-8 JSON без комментариев:

```json
{
  "id": "org_example_hello",
  "name": "Hello",
  "version": "1.0.0",
  "description": "Мой первый плагин",
  "entry": "plugin.py"
}
```

`id` в манифесте — имя, выбранное автором. Оно может состоять из букв, цифр, `_` и `-`, максимум 64 символа; **точка в текущей версии не поддерживается**. EduCode создаёт **свой уникальный ID установки** для каждой добавленной копии. Два скачанных плагина с одинаковым `id` в манифесте могут быть установлены одновременно; включение, удаление, настройки, команды и идентификаторы языков разделены по ID установки. Сам `id` в `plugin.json` остаётся полезным автору и отображается как `manifestId`.

`plugin.py`:

```python
from educode.plugin import Plugin

def register(plugin: Plugin):
    plugin.add_search('Hello: приветствие', 'hello')
    plugin.add_context_menu('Hello: приветствие', 'hello')
    plugin.add_setting('name', 'Как вас зовут?', 'Друг')
    plugin.add_sidebar('Hello', 'panel.html')
    plugin.add_language('.greet', 'greet', ['hello', 'friend'])
    plugin.add_translation('en', {'Как вас зовут?': 'Your name?'})

def run(command, context):
    if command == 'hello':
        name = context['settings'].get('name', 'Друг')
        return f'Привет, {name}! Проект: {context["project"]}'
    return 'Неизвестная команда: ' + command
```

`panel.html`:

```html
<!doctype html><html lang="ru"><meta charset="utf-8">
<style>body{background:#292b2e;color:white;font:14px sans-serif;padding:12px}</style>
<h2>Hello</h2>
<a href="panel.html?command=hello">Поздороваться</a>
```

В настройках откройте «Плагины», выберите `plugin.json` (или вставьте путь к папке) и нажмите «Добавить». Затем откройте проект, нажмите Ctrl+P и найдите `Hello: приветствие`; откройте правую панель Hello. Плагин можно выключить и удалить в настройках. После правки установленного плагина установите новую копию или удалите старую и добавьте заново: EduCode копирует исходную папку при установке.

### 3. Контракт Python API — все методы

Файл `tools/educode/plugin.py` содержит класс `Plugin`. `register(plugin)` вызывается при загрузке; она должна зарегистрировать элементы интерфейса. `run(command, context)` вызывается при выборе зарегистрированной команды. `run` может отсутствовать только у плагина, в котором нет действий. Возвращайте строку, которую IDE покажет в правой панели; возвращаемое значение должно преобразовываться в JSON.

| Метод `plugin` | Аргументы | Что получает пользователь |
|---|---|---|
| `add_setting(key, label, default='')` | Ключ, подпись, строковое значение по умолчанию | Поле на собственной странице настроек плагина. Значения в `context['settings']` доступны по исходному `key`. |
| `add_search(label, command)` | Подпись, строка команды | Пункт в глобальном поиске Ctrl+P. Выбор вызывает `run(command, context)`. |
| `add_context_menu(label, command)` | Подпись, строка команды | Пункт меню файлов проекта. Выбор вызывает `run`. |
| `add_sidebar(title, html)` | Заголовок, относительный путь к HTML внутри папки плагина | Кнопка и панель справа под базовыми панелями. Только локальный HTML внутри папки. |
| `add_language(extension, language_id, keywords=None)` | Расширение с точкой или без, имя языка, список ключевых слов | Подсветка ключевых слов, чисел и комментариев `#` в файлах этого расширения. ID языка автоматически отделяется от других установок. |
| `add_translation(locale, strings)` | Код языка, словарь `{русский текст: перевод}` | Язык в настройках и перевод известных строк интерфейса. Несколько плагинов могут предоставлять разные языки. |

`context` в `run` содержит `project` (абсолютный путь к открытой папке), `activeFile` (абсолютный путь к файлу или пустую строку) и `settings` (словарь настроек **именно этой установки**). У двух копий одного плагина настройки независимы. `run` исполняется в отдельном Python-процессе при каждом действии: храните долговременные данные в настройках, файле проекта или собственном файле пользователя, а не в глобальной переменной модуля. Для действий через работающую IDE можно дополнительно использовать `import educode` и его функции из [справочника](TERMINAL_API.md), если доступен управляющий канал.

### 4. HTML-панель, команды и файлы

HTML-файл панели загружается из папки установленного плагина. Можно использовать HTML, CSS и JavaScript для интерфейса. Чтобы передать команду Python, перейдите на URL **этого же HTML-файла** с `?command=`. IDE перехватит переход и вызовет `run`; результат будет показан под HTML. Для параметров кодируйте значение:

```html
<input id="message" placeholder="Сообщение">
<button onclick="location.href='panel.html?command='+encodeURIComponent('say:'+document.getElementById('message').value)">Отправить</button>
```

```python
def run(command, context):
    if command.startswith('say:'):
        return command[4:]
    return 'Неизвестная команда'
```

Внешняя навигация из панели блокируется. Для внешнего URL вызовите `educode.open_browser('https://...')` из `run`, если плагин работает внутри IDE. Пути `entry` и `html` должны оставаться в папке плагина. Максимальный размер скачиваемого ZIP — 5 МБ, суммарно распакованных файлов — 20 МБ; архив должен содержать ровно один `plugin.json`.

### 5. Распространение и проверка

Дайте пользователю папку плагина или ZIP с папкой внутри. ZIP можно установить по HTTP(S)-ссылке из настроек. Вместе с плагином публикуйте описание, версию, список необходимых Python-пакетов, лицензии и действия с сетью/файлами. Плагин Git в [`plugins/git`](../plugins/git) показывает рабочие регистрацию, панель и команды. Он опционален; GitHub-аккаунт ему не нужен.

Проверяйте сначала в тестовом проекте. Из исходной папки EduCode можно вызвать host вручную (`PYTHONPATH=tools` должен указывать на библиотеку): `python tools/plugin-host.py /tmp/educode-plugins-test install plugins/git`, затем `... list`. На Windows вместо `/tmp/...` используйте путь к временной папке. Не редактируйте вручную каталог установленных плагинов и `state.json`. Ошибка загрузки показывается в списке плагинов.

## English

### Quick start

A plugin is a directory containing UTF-8 `plugin.json`, an entry Python file, and optionally local HTML. Use the complete working example above. Set the manifest `id` to letters/digits/underscore/hyphen only (up to 64 characters); `org_example_hello` is valid, while dots are currently invalid. The IDE generates a separate unique **installation ID** each time a plugin is added. Therefore two plugins with the same manifest ID can coexist, and their enable state, settings, commands, and language IDs remain separate. The manifest ID is available as `manifestId`.

Use Settings → Plugins to select `plugin.json`, enter a folder path, or paste an HTTP(S) ZIP URL. Click Add, then test a search item with Ctrl+P. Disable or remove a plugin in the same settings page. EduCode copies files during installation; reinstall after changing the source. ZIP downloads are limited to 5 MB and 20 MB unpacked, and must contain exactly one manifest.

### Complete extension API

`from educode.plugin import Plugin` imports the registration API. The required `register(plugin: Plugin)` function adds features. `run(command, context)` handles actions from search, context menus, and the sidebar. It receives `project`, `activeFile`, and an installation-specific `settings` dictionary, and should return JSON-serializable output, usually a string. Each action runs in a fresh Python process.

| Method | Effect |
|---|---|
| `add_setting(key, label, default='')` | Add a text field on this plugin's Settings page. Read it from `context['settings'][key]`. |
| `add_search(label, command)` | Add an item to Ctrl+P global search. |
| `add_context_menu(label, command)` | Add an item to the project file menu. |
| `add_sidebar(title, html)` | Add a right-side panel using a local HTML file inside the plugin folder. |
| `add_language(extension, language_id, keywords=None)` | Highlight keywords, numbers, and `#` comments for an extension. |
| `add_translation(locale, strings)` | Add a language choice and map Russian source UI strings to translations. |

See the Python and HTML examples above for all method calls. A panel link to `panel.html?command=hello` invokes `run('hello', context)`; encode user-entered parameters with `encodeURIComponent`. External page navigation is blocked. For the base IDE API (`import educode`) see [Terminal and Python API](TERMINAL_API.md).

### Distribution and limits

Ship the plugin folder or a ZIP that contains it, with a description, version, dependencies, license, and a statement of file/network behavior. Only install plugins you trust: they are normal Python code running with your user permissions. The registration API only adds IDE elements and cannot remove built-in features. Use [`plugins/git`](../plugins/git) as a working example. Test in a disposable project before sharing it.
