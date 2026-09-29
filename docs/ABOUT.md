# EduCode / О проекте

## Русский

EduCode — настольная IDE для изучения Python на Windows и Linux. В базовой установке есть редактор Monaco, анализ Python через Pyright, терминал, консоль, проводник проекта, менеджер пакетов, встроенный браузер, режим преподавателя и помощник Лира. Дополнительные функции устанавливаются как Python-плагины.

Откройте существующую папку проекта или создайте новую. EduCode подготовит `.venv`, если установлен Python. Файлы `.py`, `.json`, `.md`, `.txt`, `.env`, `.gitignore` и `requirements.txt` открываются в редакторе. Markdown имеет кнопки «Код» и «Просмотр»; `requirements.txt` — «Установить все». Диагностика Pyright применяется только к Python. Кнопка разделения или Ctrl+Alt+S открывает текущий файл справа; изменения видны в обеих частях.

Основные сочетания: Ctrl+S — сохранить, Ctrl+P — поиск, F5 — запуск, Ctrl+Alt+S — разделить редактор. Сочетания можно поменять в настройках. Язык интерфейса выбирается там же. Плагины ставятся из папки с `plugin.json` или ZIP-ссылки; их можно выключить и удалить. Пример Git находится в `plugins/git` и не включается автоматически.

Сборка исходников: на Windows запустите `powershell -File tools/build.ps1`; на Linux — `bash tools/build-linux.sh`. Для сборки нужны Qt 5.15, CMake, компилятор C++17, Node.js, Python и зависимости `npm ci`. Скрипты установки и упаковки пока не являются релизным установщиком.

## English

EduCode is a desktop IDE for learning Python on Windows and Linux. The base app includes a Monaco editor, Pyright Python analysis, terminal, console, project explorer, package manager, built-in browser, Teacher Mode, and the Lyra assistant. Optional features can be added with Python plugins.

Open an existing project folder or create one. EduCode prepares a `.venv` when Python is installed. The editor supports `.py`, `.json`, `.md`, `.txt`, `.env`, `.gitignore`, and `requirements.txt`. Markdown has Code and Preview modes; `requirements.txt` offers Install all. Pyright diagnostics apply only to Python. The split button or Ctrl+Alt+S opens the current file on the right with shared edits.

Main shortcuts: Ctrl+S saves, Ctrl+P searches, F5 runs, and Ctrl+Alt+S splits the editor. Change shortcuts and interface language in Settings. Install plugins from a folder containing `plugin.json` or a ZIP URL; plugins can be disabled or removed. The optional Git example lives in `plugins/git` and is not enabled automatically.

Build from source with `powershell -File tools/build.ps1` on Windows or `bash tools/build-linux.sh` on Linux. You need Qt 5.15, CMake, a C++17 compiler, Node.js, Python, and `npm ci` dependencies. Installer creation is a later release step.
