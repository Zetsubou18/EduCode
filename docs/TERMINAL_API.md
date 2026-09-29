# Терминал и Python API / Terminal and Python API

## Русский

### Что представляет собой терминал

Вкладка «Терминал» запускает обычную оболочку в папке открытого проекта: `cmd.exe` на Windows и пользовательский shell на Linux. В `PATH` первым ставится `.venv` проекта, поэтому `python` и `pip` относятся к нему. EduCode **не вводит собственный язык команд терминала**: доступны все программы, установленные в вашей ОС. Полный список системных команд зависит от ОС и установленных пакетов; для справки используйте `help` в `cmd`, `help`/`man имя` в Linux или `команда --help`.

| Задача | Linux | Windows `cmd` |
|---|---|---|
| Текущая папка | `pwd` | `cd` |
| Список файлов | `ls -la` | `dir` |
| Сменить папку | `cd src` | `cd src` |
| Запустить программу | `python main.py` | `python main.py` |
| Версия Python | `python --version` | `python --version` |
| Пакеты окружения | `python -m pip list` | `python -m pip list` |
| Установить зависимости | `python -m pip install -r requirements.txt` | то же |
| Git-статус | `git status` | `git status` |

Команды `sudo apt install` вводятся **в системном Linux-терминале** для установки `.deb` и требуют прав администратора. Они не являются командами EduCode.

### Запуск EduCode из командной строки

| Команда или опция | Действие |
|---|---|
| `EduCode` | Открыть IDE. На Windows используйте `EduCode.exe`, если папка приложения не добавлена в `PATH`. |
| `EduCode .` | Открыть текущую папку как проект. |
| `EduCode project/` | Открыть папку проекта. |
| `EduCode main.py` | Открыть файл. |
| `EduCode first.py second.py` | Открыть несколько файлов. Путь с пробелами берите в кавычки. |
| `--line N`, `--column N` | Позиция в открываемом файле, отсчёт с 1. |
| `--stdin` или `-` | Открыть текст UTF-8 из стандартного ввода, максимум 8 МиБ. |
| `--proxy URL` | Сохранить HTTP/SOCKS5-прокси. |
| `--no-proxy` | Очистить явный прокси. |
| `--config` | Напечатать путь к конфигурации. |
| `--logs` | Напечатать путь к журналам. |
| `--verbose` | Дополнительно выводить сообщения Qt в stderr. |
| `--wait` | Ждать закрытия IDE; сейчас это поведение по умолчанию. |
| `--version`, `-v` | Версия. |
| `--help`, `-h` | Встроенная справка. |
| `--` | Последующие аргументы считать путями, даже если они начинаются с `-`. |

### Встроенная библиотека `import educode`

Она доступна Python-процессу, запущенному IDE, её консолью или терминалом. Библиотека соединяется с **уже запущенным** EduCode через локальный управляющий канал. В обычном внешнем Python без переменной `EDUCODE_CONTROL` вызовы выдадут ошибку. Ошибки операций представлены исключением `RuntimeError`.

| Функция | Аргументы | Результат и действие |
|---|---|---|
| `notify(title, message='')` | Две строки | Показать уведомление. |
| `get_settings()` | Нет | Словарь настроек, без секретного ключа Groq. |
| `configure(**changes)` | Имена ключей конфигурации | Изменить настройки и вернуть обновлённый словарь. Для ключей с точкой: `configure(**{'editor.fontSize': 16})`. |
| `command(name)` | Имя команды IDE | Выполнить команду из таблицы ниже. |
| `context()` | Нет | Словарь с `project`, `activeFile`, `activeCode`, `problems`, `console`, `terminal`, `packages`, `settings`, `log`, `time`, `os`, `architecture`. |
| `project_files()` | Нет | Список относительных путей проекта, не более 5000; `.venv` и `.git` исключены. |
| `read_file(path)` | Путь в проекте | Прочитать UTF-8 файл до 2 МиБ. |
| `write_file(path, content)` | Путь и текст | Атомарно записать файл внутри проекта. |
| `delete_file(path)` | Путь в проекте | Удалить файл через механизм IDE. |
| `console_input(text)` | Текст | Передать ввод запущенной Python-программе. |
| `terminal_input(text)` | Текст | Открыть терминал при необходимости и отправить текст. Для выполнения добавьте `\n`. |
| `package(action, name)` | `install`, `update` или `remove`; имя пакета | Запустить операцию pip в окружении проекта. |
| `open_browser(url)` | URL `http://` или `https://` | Открыть во встроенном браузере. |
| `remember(kind, key, value='')` | `kind`: `user` или `facts` | Сохранить факт Лиры; пустое значение удаляет ключ. |

Допустимые имена `command(name)`: `save`, `saveAll`, `find`, `replace`, `close`, `quickOpen`, `run`, `back`, `palette`, `split`, `terminal`, `toggleExplorer`, `notifications`, `browser`. Сочетания этих команд настраиваются в IDE.

```python
import educode

educode.notify('Готово', 'Файл обработан')
print(educode.project_files())
source = educode.read_file('main.py')
educode.write_file('result.txt', f'Длина main.py: {len(source)}\n')
educode.command('quickOpen')
```

Команды модуля из терминала: `python -m educode notify "Заголовок" "Сообщение"` и `python -m educode settings`. Вызов `python -m educode` без аргументов показывает краткую подсказку. Для расширений предназначен **отдельный** `import educode.plugin`; он описан в [руководстве по плагинам](PLUGINS.md).

## English

### Terminal and CLI

The Terminal tab runs the normal OS shell in the project directory: `cmd.exe` on Windows or the user's shell on Linux. The project `.venv` comes first in `PATH`, so `python` and `pip` use it. EduCode has **no separate terminal command language**. Available commands depend on the OS and installed software; use `help`, `man command`, or `command --help` for the full system-specific command set. The table above shows common commands on both platforms.

Run `EduCode` (or `EduCode.exe` on Windows) to open the IDE, `EduCode .` for the current folder, `EduCode project/` for a project, and `EduCode first.py second.py` for files. Quote paths containing spaces. CLI options: `--line N`, `--column N` (1-based position); `--stdin`/`-` (up to 8 MiB UTF-8 from stdin); `--proxy URL`, `--no-proxy`; `--config`, `--logs`; `--verbose`; `--wait` (already the default); `--version`/`-v`; `--help`/`-h`; and `--` to end option parsing.

### Built-in `import educode` API

The library is available to Python started by the IDE, its console, or its terminal. It talks to the running IDE over a local control channel. Calls outside EduCode without `EDUCODE_CONTROL` raise an error; failed operations raise `RuntimeError`.

| Function | Purpose |
|---|---|
| `notify(title, message='')` | Show a notification. |
| `get_settings()` | Return settings except the Groq secret. |
| `configure(**changes)` | Update settings; use `configure(**{'editor.fontSize': 16})` for dotted keys. |
| `command(name)` | Run a built-in command; supported names are listed above. |
| `context()` | Return project, active file/code, problems, consoles, packages, settings, log tail, time, OS, and architecture. |
| `project_files()` | List up to 5000 project files, excluding `.venv` and `.git`. |
| `read_file(path)` | Read a UTF-8 project file up to 2 MiB. |
| `write_file(path, content)` | Atomically write inside the project. |
| `delete_file(path)` | Delete a project file. |
| `console_input(text)` | Send input to the running Python process. |
| `terminal_input(text)` | Send text to the terminal; append `\n` to execute. |
| `package(action, name)` | Start pip `install`, `update`, or `remove` in the project environment. |
| `open_browser(url)` | Open an HTTP(S) URL in the built-in browser. |
| `remember(kind, key, value='')` | Update Lyra memory: `user` or `facts`; empty value removes a key. |

Use `python -m educode notify "Title" "Message"` or `python -m educode settings` in the IDE terminal. `python -m educode` prints brief usage. `import educode.plugin` is the separate extension API; see the [plugin guide](PLUGINS.md).
