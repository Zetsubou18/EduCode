# EduCode

**Русский** · [English](#english)

EduCode — настольная Python IDE для Windows и Linux. В основе — редактор Monaco, диагностика Pyright, терминал, файловый менеджер и необязательные плагины на Python. Интерфейс построен на C++17 и Qt 5.15; программа работает локально.

## Установка

| Система | Скачать | Установить |
| --- | --- | --- |
| Windows x64 | [EduCodeSetup-0.2.0-win64.exe](https://github.com/Zetsubou18/EduCode/releases/latest/download/EduCodeSetup-0.2.0-win64.exe) | Запустите установщик и выберите папку. По умолчанию используется `%LOCALAPPDATA%\Programs\EduCode`. |
| Ubuntu/Debian amd64 | [educode_0.2.0_amd64.deb](https://github.com/Zetsubou18/EduCode/releases/latest/download/educode_0.2.0_amd64.deb) | После скачивания выполните `sudo apt install ./educode_0.2.0_amd64.deb` из папки с файлом. |

Установщик Windows предлагает ярлыки на рабочем столе и в меню «Пуск». Закрепить приложение на панели задач можно через контекстное меню запущенной программы. Пакет Debian устанавливает пункт меню приложений и команду `EduCode`.

## Возможности

- Python с Pyright, JSON, Markdown с просмотром, TXT, `.env`, `.gitignore` и `requirements.txt`.
- Разделение редактора, поиск по проекту, встроенные терминал и браузер, помощник.
- Русский и английский интерфейс; новые языки добавляются через плагины.
- Python API для программ и отдельный API расширений. Плагины могут добавлять команды, панели, настройки, действия поиска и языки. Пример Git-плагина находится в [`plugins/git`](plugins/git).

## Сообщить об ошибке
Нашли ошибку в EduCode? Отправьте описание проблемы, логи и, если возможно, скриншот на: `jobzetsubou@gmail.com`

Документация: [о проекте](docs/ABOUT.md) · [терминал и Python API](docs/TERMINAL_API.md) · [создание плагинов](docs/PLUGINS.md) · [Linux](LINUX.md).

## Сборка из исходников

Нужны Qt 5.15 с WebEngine, CMake, компилятор C++17, Node.js и Python. Выполните `npm ci`, затем `powershell -File tools/build.ps1` на Windows или `bash tools/build-linux.sh` на Linux. Релизные пакеты собираются командами `powershell -File tools/build-installer.ps1` и `bash tools/build-deb.sh` после сборки приложения.

## English

EduCode is a local desktop Python IDE for Windows and Linux. Its core includes Monaco editing, Pyright diagnostics, a terminal, a file manager, and optional Python plugins. The interface uses C++17 and Qt 5.15.

### Install

| System | Download | Install |
| --- | --- | --- |
| Windows x64 | [EduCodeSetup-0.2.0-win64.exe](https://github.com/Zetsubou18/EduCode/releases/latest/download/EduCodeSetup-0.2.0-win64.exe) | Run the installer and choose a folder. The default is `%LOCALAPPDATA%\Programs\EduCode`. |
| Ubuntu/Debian amd64 | [educode_0.2.0_amd64.deb](https://github.com/Zetsubou18/EduCode/releases/latest/download/educode_0.2.0_amd64.deb) | From the download folder, run `sudo apt install ./educode_0.2.0_amd64.deb`. |

The Windows installer offers Desktop and Start Menu shortcuts. Pinning to the taskbar is available from the running app’s Windows context menu. The Debian package adds an application menu entry and the `EduCode` command.

### Features

- Python and Pyright; JSON; Markdown source and preview; TXT, `.env`, `.gitignore`, and `requirements.txt`.
- Split editing, project search, integrated terminal and browser, and an assistant.
- Russian and English UI, with additional language packs supplied by plugins.
- Separate Python APIs for user programs and IDE plugins. Plugins can add commands, panels, settings, search actions, and languages. See the optional [`plugins/git`](plugins/git) example.

## Report a Bug
Found a bug in EduCode? Send a description of the issue, logs, and, if possible, a screenshot to: `jobzetsubou@gmail.com`

Guides: [About](docs/ABOUT.md) · [Terminal and Python API](docs/TERMINAL_API.md) · [Plugin development](docs/PLUGINS.md) · [Linux](LINUX.md).

### Build from source

Install Qt 5.15 with WebEngine, CMake, a C++17 compiler, Node.js, and Python. Run `npm ci`, followed by `powershell -File tools/build.ps1` on Windows or `bash tools/build-linux.sh` on Linux. Build release packages with `powershell -File tools/build-installer.ps1` and `bash tools/build-deb.sh` after the application build.
