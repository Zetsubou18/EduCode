"""Package operations for the active EduCode virtual environment."""
import json, subprocess, sys, urllib.request, urllib.parse, urllib.error, os, re, tempfile, time

python, action = sys.argv[1], sys.argv[2]

def run(*args):
    return subprocess.run([python, '-m', 'pip', *args], text=True, encoding='utf-8',
                          errors='replace', capture_output=True, timeout=300)

def emit(value):
    print(json.dumps(value, ensure_ascii=False))

try:
    if action == 'list':
        result = run('list', '--format=json', '--disable-pip-version-check')
        if result.returncode: raise RuntimeError(result.stderr or result.stdout)
        items = json.loads(result.stdout)
        emit([{'name': x['name'], 'version': x['version'], 'latest': '', 'installed': True} for x in items])
    elif action == 'info':
        name = sys.argv[3].strip()
        with urllib.request.urlopen('https://pypi.org/pypi/' + urllib.parse.quote(name) + '/json', timeout=12) as response:
            payload = json.load(response)
            data = payload['info']
        description = (data.get('description') or '')[:12000]
        description = re.sub(r'<picture\b[^>]*>.*?</picture\s*>', '', description, flags=re.I | re.S)
        description = '\n'.join(line for line in description.splitlines()
                                if '![' not in line and '<img' not in line.lower() and '<source' not in line.lower())
        description = re.sub(r'<(?:img|source)\b[^>]*>', '', description, flags=re.I)
        description = re.sub(r'!\[[^\]]*\]\([^\n)]*(?:\)[^\n)]*)?\)', '', description)
        description = re.sub(r'\[\s*\]\([^\n)]*\)', '', description)
        description = re.sub(r'(?m)^\|\s*\|\s*\|\s*$\n^\|\s*-+\s*\|\s*-+\s*\|\s*$', '', description)
        description = re.sub(r'(?m)^\s*<[^>]+>\s*$', '', description)
        versions = list(payload.get('releases', {}).keys())
        try:
            from pip._vendor.packaging.version import Version
            versions.sort(key=Version, reverse=True)
        except Exception:
            versions.sort(reverse=True)
        emit({'name': data.get('name'), 'version': data.get('version'), 'summary': data.get('summary'),
              'description': description, 'descriptionContentType': data.get('description_content_type') or 'text/plain',
              'author': data.get('author') or data.get('maintainer'),
              'license': data.get('license'), 'home': data.get('project_url') or data.get('home_page'),
              'requiresPython': data.get('requires_python'), 'keywords': data.get('keywords'),
              'versions': versions[:200]})
    elif action == 'search':
        query = sys.argv[3].strip()
        if len(query) < 2: emit([]); sys.exit(0)
        cache = os.path.join(tempfile.gettempdir(), 'educode-pypi-names.txt')
        exact = None
        try:
            with urllib.request.urlopen('https://pypi.org/pypi/' + urllib.parse.quote(query) + '/json', timeout=6) as response:
                info = json.load(response)['info']
            exact = {'name': info.get('name') or query, 'version': info.get('version') or '',
                     'summary': info.get('summary') or '', 'installed': False}
        except urllib.error.HTTPError as error:
            if error.code != 404: raise
        except (urllib.error.URLError, TimeoutError):
            pass
        if exact:
            emit([exact]); sys.exit(0)
        if not os.path.exists(cache):
            req = urllib.request.Request('https://pypi.org/simple/', headers={'Accept':'application/vnd.pypi.simple.v1+json','User-Agent':'EduCode/0.1'})
            with urllib.request.urlopen(req, timeout=45) as response:
                payload = json.load(response)
            names = [item['name'] for item in payload.get('projects', [])]
            with open(cache, 'w', encoding='utf-8') as file: file.write('\n'.join(names))
        else:
            with open(cache, encoding='utf-8') as file: names = file.read().splitlines()
        needle=query.lower()
        matches=[name for name in names if needle in name.lower()]
        normalize=lambda value: re.sub(r'[-_.]+','-',value.lower())
        normalized=normalize(query)
        matches.sort(key=lambda name:(normalize(name)!=normalized,not normalize(name).startswith(normalized),len(name),name.lower()))
        out=[{'name':name,'version':'','summary':'Пакет из каталога PyPI','installed':False} for name in matches[:40]]
        for item in out[:3]:
            try:
                with urllib.request.urlopen('https://pypi.org/pypi/'+urllib.parse.quote(item['name'])+'/json',timeout=10) as response: info=json.load(response)['info']
                item.update(version=info.get('version') or '',summary=info.get('summary') or '')
            except Exception: pass
        emit(out)
    elif action == 'requirements':
        path = os.path.abspath(sys.argv[3])
        if os.path.basename(path) != 'requirements.txt' or not os.path.isfile(path):
            raise RuntimeError('Выберите существующий requirements.txt.')
        result = run('install', '-r', path, '--disable-pip-version-check')
        if result.returncode: raise RuntimeError(result.stderr or result.stdout)
        emit({'ok': True, 'message': 'Зависимости установлены из requirements.txt'})
    elif action in ('install', 'update', 'remove'):
        name = sys.argv[3].strip()
        if not name or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.[]<>=!~' for c in name):
            raise RuntimeError('Некорректное имя пакета.')
        args = ['uninstall', '-y', name] if action == 'remove' else ['install', '--upgrade', name, '--disable-pip-version-check']
        result = run(*args)
        if result.returncode: raise RuntimeError(result.stderr or result.stdout)
        emit({'ok': True, 'message': ('Удалён ' if action == 'remove' else 'Установлен ') + name})
    else: raise RuntimeError('Неизвестная операция.')
except Exception as error:
    emit({'error': str(error)[:4000]})
    sys.exit(1)
