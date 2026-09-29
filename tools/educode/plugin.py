"""Additive EduCode plugin registration API.

Plugins run as normal Python code. Install only plugins you trust.
"""
from dataclasses import dataclass, field


@dataclass
class Plugin:
    id: str
    name: str
    version: str = '1.0'
    description: str = ''
    settings: list = field(default_factory=list)
    search: list = field(default_factory=list)
    context_menu: list = field(default_factory=list)
    sidebar: dict = field(default_factory=dict)
    languages: list = field(default_factory=list)
    translations: dict = field(default_factory=dict)

    def add_setting(self, key, label, default=''):
        self.settings.append({'key': key, 'label': label, 'default': default})

    def add_search(self, label, command):
        self.search.append({'label': label, 'command': command})

    def add_context_menu(self, label, command):
        self.context_menu.append({'label': label, 'command': command})

    def add_sidebar(self, title, html):
        self.sidebar = {'title': title, 'html': html}

    def add_language(self, extension, language_id, keywords=None):
        self.languages.append({'extension': extension, 'id': language_id,
                               'keywords': keywords or []})

    def add_translation(self, locale, strings):
        self.translations[locale] = dict(strings)
