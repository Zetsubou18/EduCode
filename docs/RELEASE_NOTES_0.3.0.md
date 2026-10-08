# EduCode 0.3.0

**Русский** · [English](#english)

Это единое обновление редактора, Python-инструментов, Teacher Mode, менеджера библиотек, Лиры, установщиков Windows и Linux. Основная цель релиза — сделать ежедневную работу стабильнее, быстрее и понятнее без изменения привычного рабочего процесса.

## Главное

- Редактор больше не перехватывает стандартные команды Monaco: исправлены `Tab`, `Shift+Tab`, `Backspace`, удаление выделения и объединение строк.
- Python-подсказки получили inline-имена параметров, счётчик ссылок над функциями и классами, компактный список использований и улучшенную работу с выражениями в f-string.
- Цветовая схема Python и всплывающие окна приведены к тёмной палитре PyCharm.
- Teacher Mode сохраняет адрес конференции, объясняет причину отключения и позволяет переподключиться после кика, таймаута или сетевой ошибки.
- Groq заменён на Google Gemini. Добавлены безопасная миграция настроек, ограниченные повторы при 429/5xx и понятные сообщения об ошибках.
- Нижняя панель IDE теперь сворачивается, сохраняет высоту и автоматически раскрывается при запуске программы или выборе вкладки.
- Добавлены фоновые задачи с общим прогрессом и подробностями по каждой операции.
- Появился постоянный диагностический `AutoEDI.log` со снимком состояния IDE.

## Менеджер библиотек

- Устранена гонка поиска: устаревший ответ больше не заменяет новый запрос.
- Добавлены задержка ввода, приоритет точного совпадения, локальный кэш каталога и быстрая очистка поиска.
- Для нового пакета можно выбрать версию; обновление предлагается только при наличии более новой версии.
- Описание PyPI поддерживает безопасный Markdown. Внешние изображения, бейджи и пустые HTML-обёртки удаляются.
- После установки, обновления или удаления пакета автоматически обновляются индексы и перезапускается Pyright.

## Исправления Linux

- Терминал больше не зависит от нативного `node-pty`, собранного на конкретной Ubuntu. Используется переносимый PTY-бэкенд на Python, совместимый с Debian.
- В Debian-пакет добавлена обязательная зависимость `qml-module-qtquick-controls`, поэтому ошибка `QtQuick.Controls version 1.2 is not installed` больше не требует ручного исправления.

## Другие исправления

- Удалены настройка прокси и дублирующая глобальная память Лиры; старые данные аккуратно переносятся в рабочие факты.
- Исправлены поиск по несохранённому коду, длинные описания PyPI, копирование текста ошибок и ряд проблем интерфейса.
- Windows-установщик выполняет тяжёлые операции в фоне и остаётся отзывчивым.
- EduCode проверяет GitHub Releases при запуске и уведомляет о новой версии. Проверку можно отключить в настройках.

## Проверка

Релиз проверен на Windows и Ubuntu: сборки Release, C++-тесты, Teacher Mode, offline-тесты Gemini, UI-smoke, Pyright, консоль, PTY-терминал, менеджер библиотек и поток из 100 000 строк.

---

## English

This is one coordinated update to the editor, Python tooling, Teacher Mode, package manager, Lyra, and the Windows and Linux installers. The release focuses on stability, responsiveness, and clearer feedback while preserving the familiar workflow.

## Highlights

- Monaco's standard editing commands are no longer overridden. `Tab`, `Shift+Tab`, `Backspace`, selection deletion, and line joining work correctly.
- Python assistance now includes inline parameter names, reference counts above functions and classes, a compact usages popup, and better f-string expression support.
- Python syntax colors, completion, and hover windows now follow a dark PyCharm-inspired palette.
- Teacher Mode remembers the conference address, explains disconnects, and supports reconnecting after kicks, timeouts, and network failures.
- Google Gemini replaces Groq, with safe settings migration, bounded retries for 429/5xx responses, and clearer errors.
- The bottom IDE panel can be collapsed, remembers its height, and opens automatically when needed.
- Background operations now expose combined progress and per-task details.
- A persistent `AutoEDI.log` records an atomic snapshot of the IDE state for diagnostics.

## Package manager

- Stale search responses can no longer overwrite newer queries.
- Search now has debouncing, exact-match priority, a local package-name cache, and a clear button.
- A version can be selected before installation, and Update appears only when a newer release exists.
- PyPI descriptions support sanitized Markdown without remote images, badges, or empty HTML wrappers.
- Package changes automatically refresh project and Python indexes and restart Pyright.

## Linux fixes

- The terminal no longer relies on an Ubuntu-built native `node-pty` binary. A portable Python PTY backend works across Ubuntu and Debian.
- The Debian package now depends on `qml-module-qtquick-controls`, preventing the `QtQuick.Controls version 1.2 is not installed` startup error.

## Additional fixes

- The proxy setting and duplicate global Lyra memory section were removed; existing memory is migrated safely into working facts.
- Unsaved-code search, long PyPI descriptions, problem-message copying, and several UI edge cases were fixed.
- The Windows installer performs heavy work in the background and remains responsive.
- EduCode checks GitHub Releases on startup and can notify users about a newer version. The check can be disabled in Settings.

## Validation

The release was validated on Windows and Ubuntu with Release builds, C++ tests, Teacher Mode tests, offline Gemini tests, UI smoke tests, Pyright, the Python console, the PTY terminal, package management, and a 100,000-line output stress test.
