"""HTTP/SOCKS transport for Gemini's OpenAI-compatible endpoint. No SDK required."""
import contextlib
import hashlib
import json
import os
import re
import time
import urllib.error
import urllib.request
from pathlib import Path


def duration(value):
    try:
        return max(0.0, float(value))
    except (TypeError, ValueError):
        return sum(float(n) * {'ms': .001, 's': 1, 'm': 60, 'h': 3600, 'd': 86400}[unit]
                   for n, unit in re.findall(r'([\d.]+)(ms|s|m|h|d)', str(value)))


def build_opener():
    return urllib.request.build_opener()


class Quota:
    def __init__(self, directory, key, model, limits, status=lambda text: None, clock=time.time, sleep=time.sleep):
        fingerprint = hashlib.sha256((key+'\0'+model).encode()).hexdigest()[:24]
        self.path = Path(directory)/('quota-'+fingerprint+'.json')
        self.lock = self.path.with_suffix('.lock')
        self.limits = limits
        self.status, self.clock, self.sleep = status, clock, sleep
        self.path.parent.mkdir(parents=True, exist_ok=True)

    @contextlib.contextmanager
    def transaction(self):
        started = time.monotonic()
        while True:
            try:
                self.lock.mkdir()
                break
            except FileExistsError:
                try:
                    if time.time()-self.lock.stat().st_mtime > 60: self.lock.rmdir()
                except FileNotFoundError: pass
                if time.monotonic()-started > 10: raise RuntimeError('Не удалось получить счётчик лимитов ИИ.')
                self.sleep(.05)
        try:
            try: data = json.loads(self.path.read_text(encoding='utf-8'))
            except FileNotFoundError: data = {}
            except (ValueError, OSError): raise RuntimeError('Счётчик лимитов повреждён: '+str(self.path)) from None
            yield data
            temporary = self.path.with_suffix('.tmp')
            temporary.write_text(json.dumps(data), encoding='utf-8')
            os.replace(temporary, self.path)
        finally: self.lock.rmdir()

    def reserve(self, tokens):
        rpm, rpd, tpm, tpd = self.limits
        if tokens > tpm: raise RuntimeError('Контекст превышает лимит токенов в минуту. Сократите запрос или начните новый чат.')
        while True:
            now = self.clock()
            with self.transaction() as data:
                entries = [e for e in data.get('entries', []) if e['at'] > now-86400]
                data['entries'] = entries
                if len(entries) >= rpd or sum(e['tokens'] for e in entries)+tokens > tpd:
                    raise RuntimeError('Достигнут локальный суточный лимит ИИ. Повтор будет доступен после освобождения квоты.')
                recent = [e for e in entries if e['at'] > now-60]
                delay = max(0, data.get('blockedUntil',0)-now,
                            (entries[-1]['at']+60/rpm+.1-now) if entries else 0)
                if len(recent) >= rpm or sum(e['tokens'] for e in recent)+tokens > tpm:
                    delay = max(delay, recent[0]['at']+60.1-now)
                if delay <= 0:
                    ticket = {'at': now, 'tokens': tokens}
                    entries.append(ticket)
                    return now
            if delay > 300: raise RuntimeError('Квота ИИ исчерпана. Повторите позже; автоматическое ожидание больше 5 минут отключено.')
            self.wait(delay)

    def wait(self, seconds):
        end = self.clock()+seconds
        while self.clock() < end:
            remaining = end-self.clock()
            self.status('Жду квоту ИИ · '+str(max(1,int(remaining+.999)))+' с')
            self.sleep(min(remaining, 1))

    def update(self, ticket, headers, actual=None, cooldown=0):
        now = self.clock()
        with self.transaction() as data:
            if actual is not None:
                for entry in data.get('entries',[]):
                    if entry['at'] == ticket: entry['tokens'] = max(0, int(actual)); break
            for name in ('requests', 'tokens'):
                if headers.get('x-ratelimit-remaining-'+name) == '0':
                    cooldown = max(cooldown,duration(headers.get('x-ratelimit-reset-'+name)))
            data['blockedUntil'] = max(data.get('blockedUntil',0),now+cooldown)


def fit_context(body, tpm, output):
    body = dict(body, messages=[dict(m) for m in body['messages']])
    def estimate(): return (len(json.dumps(body, ensure_ascii=False))+2)//3+output
    for message in body['messages']:
        if message['role'] == 'tool' and len(message.get('content','')) > 4000:
            message['content'] = message['content'][:4000]+'\n[Результат сокращён. Читайте нужные файлы отдельно.]'
    # Remove complete older turns, never orphan tool_call_id messages.
    users = [i for i,m in enumerate(body['messages']) if m['role']=='user']
    while estimate() > tpm and len(users) > 1:
        body['messages'] = body['messages'][:1]+body['messages'][users[1]:]
        users = [i for i,m in enumerate(body['messages']) if m['role']=='user']
    return body, estimate()


def gemini_request(body, key, status=lambda text: None):
    opener = build_opener()
    req = urllib.request.Request('https://generativelanguage.googleapis.com/v1beta/openai/chat/completions',
        data=json.dumps(body,ensure_ascii=False).encode(),
        headers={'Content-Type':'application/json','User-Agent':'EduCode/0.3 (Gemini client)','Authorization':'Bearer '+key})
    attempts = 4
    for attempt in range(attempts):
        try:
            with opener.open(req,timeout=120) as response:
                return json.load(response)['choices'][0]['message']
        except urllib.error.HTTPError as error:
            retryable = error.code == 429 or 500 <= error.code <= 599
            raw = b''
            try: raw = error.read(8192)
            except (AttributeError, OSError): pass
            try: detail = json.loads(raw.decode('utf-8','replace')).get('error',{}).get('message','')
            except (ValueError, AttributeError): detail = ''
            if not retryable:
                if error.code == 404:
                    raise RuntimeError('Gemini не нашёл модель «'+str(body.get('model',''))+'» (HTTP 404). Выберите доступную модель Gemini в настройках.') from None
                raise RuntimeError('Gemini вернул HTTP '+str(error.code)+': '+(detail[:700] or 'проверьте API-ключ и настройки сервиса.')) from None
            delay = min(30, max(2.1,duration(error.headers.get('retry-after')) or 2 ** (attempt + 1)))
            if attempt == attempts-1:
                reason = 'ограничивает частоту запросов' if error.code == 429 else 'временно перегружен'
                raise RuntimeError('Gemini '+reason+' (HTTP '+str(error.code)+') после '+str(attempts)+' попыток. Нажмите «Повторить запрос» позже.') from None
            reason = 'ограничил запросы' if error.code == 429 else 'временно недоступен (HTTP '+str(error.code)+')'
            status(reason+'; повтор '+str(attempt+2)+'/'+str(attempts)+' через '+str(int(delay+.999))+' с…')
            time.sleep(delay)
        except (urllib.error.URLError, TimeoutError, OSError) as error:
            if attempt == attempts-1:
                raise RuntimeError('Нет соединения с Gemini после '+str(attempts)+' попыток. Проверьте VPN и доступ к сети.') from None
            delay = min(15, 2 ** (attempt + 1))
            status('Сбой сети Gemini; повтор '+str(attempt+2)+'/'+str(attempts)+' через '+str(delay)+' с…')
            time.sleep(delay)
    raise RuntimeError('Gemini не ответил')
