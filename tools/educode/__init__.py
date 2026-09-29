"""Small local EduCode API; available in IDE console, terminal and system console."""
import json
import os
import socket
import sys

def _request(method, params):
    endpoint = os.environ.get('EDUCODE_CONTROL')
    if not endpoint:
        raise RuntimeError('Запустите команду из EduCode или задайте EDUCODE_CONTROL.')
    with open(endpoint, encoding='utf-8') as file:
        control = json.load(file)
    request = {'token': control['token'], 'method': method, 'params': params}
    with socket.create_connection(('127.0.0.1', control['port']), timeout=5) as client:
        client.sendall(json.dumps(request).encode('utf-8') + b'\n')
        response = client.makefile('rb').readline(1048576)
    reply = json.loads(response)
    if not reply.get('ok'):
        raise RuntimeError(reply.get('error', 'Операция не выполнена'))
    return reply.get('result', True)

def notify(title, message=''):
    return _request('notify', {'title': str(title), 'message': str(message)})

def get_settings():
    return _request('config.get', {})

def configure(**changes):
    return _request('config.patch', changes)

def command(name):
    return _request('command', {'name': name})

def context():
    """Current project, active code, diagnostics, consoles, packages and IDE/system facts."""
    return _request('ai.context', {})

def project_files():
    return _request('project.list', {})

def read_file(path):
    return _request('project.read', {'path': str(path)})

def write_file(path, content):
    return _request('project.write', {'path': str(path), 'content': str(content)})

def delete_file(path):
    return _request('project.delete', {'path': str(path)})

def console_input(text):
    return _request('console.input', {'text': str(text)})

def terminal_input(text):
    return _request('terminal.input', {'text': str(text)})

def package(action, name):
    if action not in ('install', 'update', 'remove'):
        raise ValueError('action: install, update или remove')
    return _request('package.action', {'action': action, 'name': str(name)})

def open_browser(url):
    return _request('browser.open', {'url': str(url)})

def remember(kind, key, value=''):
    """Update global AI memory. kind is 'user' or 'facts'; empty value deletes a key."""
    return _request('memory.update', {'kind': kind, 'key': str(key), 'value': str(value)})

def main():
    if len(sys.argv) >= 3 and sys.argv[1] == 'notify':
        notify(sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else '')
    elif len(sys.argv) == 2 and sys.argv[1] == 'settings':
        print(json.dumps(get_settings(), ensure_ascii=False, indent=2))
    else:
        print('python -m educode notify "Заголовок" "Сообщение"\npython -m educode settings')
