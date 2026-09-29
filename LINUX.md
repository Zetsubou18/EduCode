# Ubuntu: подготовка и подключение

Актуальная версия собрана и проверена на ноутбуке Job с Ubuntu 24.04.1 LTS, Qt 5.15.16 и Node.js 18.19.1. Исходники: `/home/zetsubou/EduCode`. Команда установлена в `/usr/local/bin/EduCode` и указывает на текущую сборку; при повторной сборке команда автоматически запускает обновлённую версию. Есть ярлык в меню приложений.

```bash
EduCode                         # открыть IDE
EduCode test.py                 # открыть существующий файл из текущей папки
EduCode "пример программы.py"   # имя с пробелами
EduCode test.py second.py        # несколько файлов одного проекта
EduCode .                       # открыть текущую папку как проект
EduCode /path/to/project
EduCode --help
EduCode --version
EduCode -- -example.py           # имя, начинающееся с дефиса
```

Для файла IDE ищет ближайший родительский проект с `.venv`. Если среды ещё нет, файл открывается и предлагается подготовить окружение. Несуществующий путь сообщает об ошибке и возвращает код 2. Относительные пути отсчитываются от текущей папки терминала.

Проверены: открытие нескольких файлов, Python с интерактивным вводом, диагностика Pyright, обновление кода после изменений агентом, терминал Linux с `.venv`, локальный API и визуальный запуск. CoreTests: 9 passed, 0 failed, 0 skipped. Проверка: `python3 tests/linux-smoke.py` внутри графического сеанса пользователя.

Для обновления уже перенесённых исходников:
```bash
cd ~/EduCode
bash tools/build-linux.sh
```
Для первичной установки команды после сборки: `bash tools/install-linux.sh`. Скрипт не меняет существующую установку другого приложения с тем же именем.
Для подготовки новой Ubuntu Desktop в каталоге исходников (проверено на 24.04):
```bash
bash tools/bootstrap-linux.sh
bash tools/build-linux.sh
bash tools/run-linux.sh
```
Скрипт подготовки устанавливает зависимости через apt и npm; сборка по умолчанию использует 2 потока для слабого ноутбука. Windows .venv и node_modules переносить нельзя: создаются заново на Linux. Qt WebEngine требует графический сеанс обычного пользователя; не запускайте IDE через sudo.

## 1. На ноутбуке Ubuntu
Оба компьютера подключите к одной локальной сети.
```bash
sudo apt update
sudo apt install openssh-server
sudo systemctl enable --now ssh
sudo ufw allow OpenSSH
systemctl status ssh --no-pager
whoami
hostname -I
```
Запишите имя пользователя и локальный IP, например 192.168.1.42.
Документация: https://help.ubuntu.com/community/SSH/OpenSSH/InstallingConfiguringTesting

## 2. На Windows 11 в PowerShell
Замените USER и IP своими значениями:
```powershell
ssh -V
ssh-keygen -t ed25519 -f "$env:USERPROFILE/.ssh/educode_ubuntu"
Get-Content "$env:USERPROFILE/.ssh/educode_ubuntu.pub" | ssh USER@192.168.1.42 'umask 077; mkdir -p ~/.ssh; cat >> ~/.ssh/authorized_keys'
ssh -i "$env:USERPROFILE/.ssh/educode_ubuntu" USER@192.168.1.42 'uname -a'
```
При первом соединении подтвердите отпечаток сервера после проверки на Ubuntu:
```bash
ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub
```
Пароль Ubuntu вводится вами при установке публичного ключа. Закрытый ключ никуда не отправляйте.
Если ssh отсутствует, PowerShell от администратора:
```powershell
Add-WindowsCapability -Online -Name OpenSSH.Client~~~~0.0.1.0
```
Для автономных последующих команд можно загрузить ключ в ssh-agent (PowerShell администратора для первых двух команд):
```powershell
Set-Service ssh-agent -StartupType Automatic
Start-Service ssh-agent
ssh-add "$env:USERPROFILE/.ssh/educode_ubuntu"
```

## 3. Обмен исходниками без упаковки
В PowerShell, из папки EduCode:
```powershell
ssh -i "$env:USERPROFILE/.ssh/educode_ubuntu" USER@192.168.1.42 'mkdir -p ~/EduCode'
scp -i "$env:USERPROFILE/.ssh/educode_ubuntu" -r src qml web assets tools docs plugins EduCode tests CMakeLists.txt package.json package-lock.json USER@192.168.1.42:~/EduCode/
```
На Ubuntu затем выполните команды подготовки и сборки из ~/EduCode. Повторный scp обновляет файлы, но не удаляет устаревшие.

## 4. Что дать мне следующим сообщением
Имя пользователя Ubuntu, локальный IP, путь ~/EduCode и разрешение подключаться для сборки и проверки. Пароль присылать не нужно. Ноутбук должен быть включён и доступен в сети.
SSH позволяет выполнять команды и передавать файлы. Он сам по себе не показывает рабочий стол. Для проверки интерфейса можно получать скриншоты встроенным режимом EduCode: в локальном терминале графического сеанса Ubuntu:
```bash
EDUCODE_SCREENSHOT="$HOME/educode.png" bash tools/run-linux.sh
```
Получить файл на Windows:
```powershell
scp -i "$env:USERPROFILE/.ssh/educode_ubuntu" USER@192.168.1.42:~/educode.png .
```
Переменные DISPLAY/Wayland и права графического сеанса зависят от Ubuntu; при запуске через SSH их проверим на ноутбуке. Постоянное управление рабочим столом потребует отдельного удалённого рабочего стола, SSH не заменяет его.


### Исправление запуска в Wayland

В Ubuntu также требуются `qml-module-qtquick-controls` и `qml-module-qtgraphicaleffects`: QtQuick.Dialogs использует Controls 1 при отсутствии подходящего native-dialog backend. Они включены в `tools/bootstrap-linux.sh` и уже установлены на ноутбуке.
Проверка меню и обычного запуска без принудительного X11: `python3 tests/linux-launch.py`. Полный `tests/linux-smoke.py` теперь также использует окружение рабочего стола вместо принудительного X11.
Если окно не появляется, смотрите `~/.local/share/EduCode/EduCode/logs/`; предупреждения и ошибки Qt теперь также выводятся в stderr при запуске из терминала.
