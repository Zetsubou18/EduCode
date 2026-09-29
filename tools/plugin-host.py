"""Process boundary for loading optional user plugins."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import urllib.request
import zipfile
import uuid

from educode.plugin import Plugin

root = Path(sys.argv[1]).resolve()
root.mkdir(parents=True, exist_ok=True)
action = sys.argv[2]


def load(folder):
    manifest = json.loads((folder / 'plugin.json').read_text(encoding='utf-8'))
    ident = manifest['id']
    if not ident.replace('-', '').replace('_', '').isalnum() or len(ident) > 64:
        raise ValueError('Invalid plugin id')
    obj = Plugin(ident, manifest.get('name', ident), manifest.get('version', '1.0'),
                 manifest.get('description', ''))
    entry = folder / manifest.get('entry', 'plugin.py')
    if not entry.resolve().is_relative_to(folder.resolve()):
        raise ValueError('Plugin entry is outside plugin directory')
    spec = importlib.util.spec_from_file_location('educode_user_plugin_' + folder.name, entry)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.register(obj)
    return obj, module


def state():
    path = root / 'state.json'
    return json.loads(path.read_text(encoding='utf-8')) if path.exists() else {}


def save_state(data):
    (root / 'state.json').write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')


def folder_for(installation_id):
    folder = (root / installation_id).resolve()
    if folder.parent != root or not (folder / 'plugin.json').is_file():
        raise ValueError('Plugin installation not found')
    return folder


def copy_install(folder):
    obj, _ = load(folder)
    installation_id = uuid.uuid4().hex
    target = root / installation_id
    shutil.copytree(folder, target)
    return {'ok': True, 'id': installation_id, 'manifestId': obj.id}


try:
    if action == 'list':
        disabled = state()
        plugins = []
        for folder in sorted(root.iterdir()):
            if not folder.is_dir() or not (folder / 'plugin.json').exists():
                continue
            try:
                manifest = json.loads((folder / 'plugin.json').read_text(encoding='utf-8'))
                row = {'id': folder.name, 'manifestId': manifest['id'],
                       'name': manifest.get('name', manifest['id']),
                       'version': manifest.get('version', '1.0'), 'enabled': disabled.get(folder.name, True)}
                if row['enabled']:
                    obj, _ = load(folder)
                    for setting in obj.settings:
                        setting['key'] = folder.name + ':' + setting['key']
                    for language in obj.languages:
                        language['id'] = folder.name + '_' + language['id']
                    if obj.sidebar:
                        html = (folder / obj.sidebar['html']).resolve()
                        if not html.is_relative_to(folder.resolve()) or not html.is_file():
                            raise ValueError('Invalid sidebar HTML')
                        obj.sidebar['url'] = html.as_uri()
                    row.update(settings=obj.settings, search=obj.search, contextMenu=obj.context_menu,
                               sidebar=obj.sidebar, languages=obj.languages, translations=obj.translations)
                plugins.append(row)
            except Exception as exc:
                plugins.append({'id': folder.name, 'name': folder.name, 'enabled': False, 'error': str(exc)})
        print(json.dumps(plugins, ensure_ascii=False))
    elif action == 'install':
        source = sys.argv[3]
        if source.startswith(('https://', 'http://')):
            with urllib.request.urlopen(source, timeout=20) as response:
                data = response.read(5_000_001)
            if len(data) > 5_000_000:
                raise ValueError('Download exceeds 5 MB')
            import io
            archive = zipfile.ZipFile(io.BytesIO(data))
            if sum(item.file_size for item in archive.infolist()) > 20_000_000:
                raise ValueError('Unpacked plugin exceeds 20 MB')
            import tempfile
            with tempfile.TemporaryDirectory() as temporary:
                temp = Path(temporary)
                for item in archive.infolist():
                    dest = (temp / item.filename).resolve()
                    if not dest.is_relative_to(temp.resolve()):
                        raise ValueError('Unsafe archive path')
                    archive.extract(item, temp)
                candidates = list(temp.rglob('plugin.json'))
                if len(candidates) != 1:
                    raise ValueError('Archive must contain one plugin.json')
                folder = candidates[0].parent
                print(json.dumps(copy_install(folder)))
        else:
            path = Path(source).resolve()
            folder = path if path.is_dir() else path.parent
            print(json.dumps(copy_install(folder)))
    elif action == 'toggle':
        ident, enabled = sys.argv[3], sys.argv[4] == '1'
        folder_for(ident)
        data = state(); data[ident] = enabled; save_state(data)
        print(json.dumps({'ok': True}))
    elif action == 'remove':
        ident = sys.argv[3]
        folder = folder_for(ident)
        shutil.rmtree(folder)
        data = state(); data.pop(ident, None); save_state(data)
        print(json.dumps({'ok': True}))
    elif action == 'run':
        ident, command = sys.argv[3:5]
        folder = folder_for(ident)
        if not state().get(ident, True):
            raise ValueError('Plugin disabled')
        _, module = load(folder)
        context = json.loads(sys.argv[5])
        prefix = ident + ':'
        context['settings'] = {key[len(prefix):]: value for key, value in context.get('settings', {}).items()
                               if key.startswith(prefix)}
        result = module.run(command, context)
        print(json.dumps({'ok': True, 'result': result}, ensure_ascii=False))
except Exception as exc:
    print(json.dumps({'ok': False, 'error': str(exc)}, ensure_ascii=False))
    sys.exit(1)
