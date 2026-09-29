"""Wrap static Russian QML labels with the runtime language lookup once."""
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
pattern = re.compile(r'(?<![\w.])(text|hint|placeholderText|title):\s*("(?:\\.|[^"\\])*")')
for name in ('Main.qml', 'pages/Ide.qml', 'pages/Settings.qml',
             'pages/RightDock.qml', 'pages/Welcome.qml',
             'components/SearchOverlay.qml'):
    path = root / 'qml' / name
    data = path.read_text(encoding='utf-8')
    def replace(match):
        value = match.group(2)
        if not re.search('[А-Яа-яЁё]', value):
            return match.group(0)
        return f'{match.group(1)}: backend.translate({value},backend.configuration["general.language"])'
    path.write_text(pattern.sub(replace, data), encoding='utf-8')
