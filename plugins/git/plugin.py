"""Optional local Git integration; no GitHub account needed."""
import subprocess
from pathlib import Path


def register(plugin):
    plugin.add_sidebar('Git', 'panel.html')
    plugin.add_search('Git: статус', 'status')
    plugin.add_search('Git: история', 'log')
    plugin.add_context_menu('Git: статус', 'status')
    plugin.add_setting('git.showUntracked', 'Показывать новые файлы', 'yes')


def run(command, context):
    project = context.get('project', '')
    if not Path(project).is_dir():
        return 'Откройте проект.'
    fixed = {
        'status': ['status', '--short', '--branch'],
        'log': ['log', '-8', '--oneline', '--decorate'],
        'branches': ['branch', '-a'],
        'diff': ['diff', '--stat'],
        'fetch': ['fetch'],
        'add': ['add', '-A'],
    }
    if command.startswith('commit:'):
        message = command[7:].strip()
        if not message or len(message) > 200:
            return 'Введите сообщение коммита (до 200 символов).'
        args = ['commit', '-m', message]
    elif command in fixed:
        args = fixed[command]
    else:
        return 'Неизвестная команда.'
    try:
        result = subprocess.run(['git', '-C', project, *args], text=True,
                                encoding='utf-8', errors='replace', capture_output=True,
                                timeout=60)
    except FileNotFoundError:
        return 'Git не установлен или не найден в PATH.'
    except subprocess.TimeoutExpired:
        return 'Превышено время ожидания Git.'
    return (result.stdout + result.stderr).strip() or ('Готово.' if result.returncode == 0 else 'Ошибка Git.')
