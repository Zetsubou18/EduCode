"""Append current CLI help to the user's existing Linux notes, preserving their data."""
from pathlib import Path
import json

config=Path.home()/'.config/EduCode/EduCode/config.json'
data=json.loads(config.read_text(encoding='utf-8')) if config.exists() else {}
data['network.proxy']='socks5://192.168.1.11:10808'
config.parent.mkdir(parents=True,exist_ok=True)
temporary=config.with_suffix('.new')
temporary.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
temporary.chmod(0o600)
temporary.replace(config)

notes=Path.home()/'Рабочий стол/InfoLinux.txt'
marker='\n\n=== EduCode: команды и обновление 27.09.2026 ===\n'
original=notes.read_text(encoding='utf-8')
original=original.split(marker)[0]
section='''
Запуск IDE из меню или терминала:
  EduCode
  EduCode .
  EduCode test.py
  EduCode main.py test.py
  EduCode --line 15 --column 4 main.py
  EduCode --wait main.py
  EduCode --help
  EduCode --version
Относительные пути вычисляются от текущей папки. --wait означает ожидание
закрытия IDE; сейчас это стандартное поведение каждого запуска.
Пути с пробелами заключайте в кавычки. Для имени с дефисом: EduCode -- -file.py

Текст из другой команды прямо в редактор (UTF-8, максимум 8 МиБ):
  printf 'Привет\\n' | EduCode --stdin
  git diff | EduCode -
Файл stdin.txt временный: сохраните нужный текст через «Сохранить как».

Прокси (сохраняется в настройках, перезапустите IDE для встроенного браузера):
  EduCode --proxy socks5://192.168.1.11:10808
  EduCode --proxy http://192.168.1.11:10809
  EduCode --no-proxy
То же доступно в Настройки → Общие → Прокси.
При обновлении SOCKS5 LAN-адрес недоступен: Windows Xray слушает только
127.0.0.1:10808/10809. Нужно разрешить LAN и перезапустить VPN-клиент.
Альтернатива: SSH-туннель, запускать на Windows и держать соединение открытым:
  ssh -N -o ExitOnForwardFailure=yes -o ServerAliveInterval=30 -R 127.0.0.1:10808:127.0.0.1:10808 zetsubou@192.168.1.12
Тогда на Linux выбрать socks5://127.0.0.1:10808. Windows и VPN должны работать.

Логи и настройки:
  EduCode --logs
  EduCode --config
  EduCode --verbose > ~/educode-output.txt 2>&1
При обычном запуске предупреждения Qt записываются в логи; ошибки запуска
остаются видны. --verbose включает подробный вывод в терминал.
Вывод любой команды сразу в файл без nano:
  команда > ~/output.txt 2>&1
Одновременно показать и сохранить:
  команда 2>&1 | tee ~/output.txt
Добавить к существующему файлу: команда >> ~/output.txt 2>&1

Лира / Groq:
30 запросов/мин, 1000/сутки; по умолчанию 8000 токенов/мин и 200000/сутки.
Точные лимиты зависят от модели и организации; настройте их в разделе ИИ.
Каждый шаг агента и автоматический повтор учитываются; при 429 ожидается
Retry-After сервера. Не более двух автоматических повторов, затем кнопка
«Повторить запрос». Счётчик сохраняется между запусками отдельно на каждом ПК;
общую квоту Windows + Linux и других клиентов окончательно определяет Groq.
Информация: https://console.groq.com/docs/rate-limits
Локальные проверки не расходовали Groq API.
'''
notes.write_text(original+marker+section,encoding='utf-8')
print('Updated Linux proxy setting and InfoLinux.txt; original notes preserved.')
