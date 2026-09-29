"""Index source files without importing or executing any installed package."""
import json
import os
import sys

results = []
seen = set()
skip = {'__pycache__', 'tests', 'test', '.git', '.venv', 'node_modules'}

def add(title, path):
    path = os.path.abspath(path)
    if path in seen or len(results) >= 30000:
        return
    seen.add(path)
    results.append({'title': title, 'detail': path, 'path': path, 'group': 'Библиотеки Python'})

roots = {os.path.normcase(os.path.abspath(base)) for base in sys.path if os.path.isdir(base)}
for base in sys.path:
    if not os.path.isdir(base):
        continue
    for directory, folders, files in os.walk(base, followlinks=False):
        folders[:] = sorted(name for name in folders if name not in skip and name != 'site-packages' and os.path.normcase(os.path.abspath(os.path.join(directory, name))) not in roots and not name.endswith(('.dist-info', '.egg-info')) and not os.path.islink(os.path.join(directory, name)))
        # site-packages has its own sys.path entry.
        if os.path.abspath(directory) == os.path.abspath(base):
            folders[:] = [name for name in folders if name != 'site-packages']
        for name in sorted(files):
            if name.endswith(('.py', '.pyi')):
                relative = os.path.relpath(os.path.join(directory, name), base)
                title = os.path.splitext(relative)[0].replace(os.sep, '.')
                if title.endswith('.__init__'):
                    title = title[:-9]
                add(title, os.path.join(directory, name))
        if len(results) >= 30000:
            break

# Compiled and built-in modules are editable as their shipped typing stubs.
stubs = sys.argv[1] if len(sys.argv) > 1 else ''
if os.path.isdir(stubs):
    import importlib.machinery
    names = dict.fromkeys(sys.builtin_module_names, sys.executable)
    for base in sys.path:
        if os.path.isdir(base):
            for name in os.listdir(base):
                for suffix in importlib.machinery.EXTENSION_SUFFIXES:
                    if name.endswith(suffix):
                        names[name[:-len(suffix)]] = os.path.join(base, name)
    for name in sorted(names):
        path = os.path.join(stubs, name + '.pyi')
        if os.path.isfile(path):
            add(name, path)
        elif len(results) < 30000:
            results.append({'title': name, 'detail': names[name] + ' · бинарный модуль, Enter — открыть папку', 'revealPath': names[name], 'group': 'Библиотеки Python'})
print(json.dumps({'items': results, 'watch': [base for base in sys.path if os.path.isdir(base)]}))
