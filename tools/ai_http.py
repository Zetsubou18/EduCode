"""HTTP/SOCKS transport and persistent, conservative Groq quotas. No SDK required."""
import contextlib
import hashlib
import http.client
import json
import os
import re
import socket
import ssl
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path


def duration(value):
    try:
        return max(0.0, float(value))
    except (TypeError, ValueError):
        return sum(float(n) * {'ms': .001, 's': 1, 'm': 60, 'h': 3600, 'd': 86400}[unit]
                   for n, unit in re.findall(r'([\d.]+)(ms|s|m|h|d)', str(value)))


def socks_connect(proxy, host, port, timeout):
    parsed = urllib.parse.urlsplit(proxy)
    sock = socket.create_connection((parsed.hostname, parsed.port or 1080), timeout)
    def read(count):
        data = b''
        while len(data) < count:
            chunk = sock.recv(count-len(data))
            if not chunk: raise OSError('SOCKS5: соединение закрыто прокси')
            data += chunk
        return data
    try:
        sock.sendall(b'\x05\x02\x00\x02' if parsed.username else b'\x05\x01\x00')
        version, method = read(2)
        if version != 5 or method not in (0, 2): raise OSError('SOCKS5: прокси отказал в подключении')
        if method == 2:
            user = urllib.parse.unquote(parsed.username or '').encode()
            password = urllib.parse.unquote(parsed.password or '').encode()
            if max(len(user), len(password)) > 255: raise OSError('SOCKS5: слишком длинные учётные данные')
            sock.sendall(b'\x01'+bytes([len(user)])+user+bytes([len(password)])+password)
            if read(2) != b'\x01\x00': raise OSError('SOCKS5: неверные учётные данные')
        name = host.encode('idna')
        if len(name) > 255: raise OSError('SOCKS5: неверное имя сервера')
        # DNS is resolved on the proxy, avoiding local DNS routing/leaks.
        sock.sendall(b'\x05\x01\x00\x03'+bytes([len(name)])+name+int(port).to_bytes(2,'big'))
        version, result, _, address_type = read(4)
        if version != 5 or result: raise OSError('SOCKS5: сервер недоступен, код '+str(result))
        read({1: 4, 4: 16}.get(address_type, read(1)[0] if address_type == 3 else 0))
        read(2)
        return sock
    except BaseException:
        sock.close()
        raise


def build_opener(proxy=''):
    if not proxy:
        return urllib.request.build_opener()  # Respect standard system HTTP(S) proxy variables.
    if proxy.startswith(('socks5://', 'socks5h://')):
        class Connection(http.client.HTTPConnection):
            def connect(self): self.sock = socks_connect(proxy, self.host, self.port, self.timeout)
        class SecureConnection(http.client.HTTPSConnection):
            def connect(self):
                sock = socks_connect(proxy, self.host, self.port, self.timeout)
                self.sock = self._context.wrap_socket(sock, server_hostname=self.host)
        class HTTP(urllib.request.HTTPHandler):
            def http_open(self, req): return self.do_open(Connection, req)
        class HTTPS(urllib.request.HTTPSHandler):
            def https_open(self, req): return self.do_open(SecureConnection, req, context=self._context)
        return urllib.request.build_opener(urllib.request.ProxyHandler({}), HTTP(), HTTPS())
    if not proxy.startswith('http://'): raise ValueError('Прокси должен быть http:// или socks5://')
    return urllib.request.build_opener(urllib.request.ProxyHandler({'http': proxy, 'https': proxy}))


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
                    raise RuntimeError('Достигнут локальный суточный лимит Groq. Повтор будет доступен после освобождения квоты.')
                recent = [e for e in entries if e['at'] > now-60]
                delay = max(0, data.get('blockedUntil',0)-now,
                            (entries[-1]['at']+60/rpm+.1-now) if entries else 0)
                if len(recent) >= rpm or sum(e['tokens'] for e in recent)+tokens > tpm:
                    delay = max(delay, recent[0]['at']+60.1-now)
                if delay <= 0:
                    ticket = {'at': now, 'tokens': tokens}
                    entries.append(ticket)
                    return now
            if delay > 300: raise RuntimeError('Квота Groq исчерпана. Повторите позже; автоматическое ожидание больше 5 минут отключено.')
            self.wait(delay)

    def wait(self, seconds):
        end = self.clock()+seconds
        while self.clock() < end:
            remaining = end-self.clock()
            self.status('Жду квоту Groq · '+str(max(1,int(remaining+.999)))+' с')
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


def groq_request(body, key, proxy, directory, limits, status=lambda text: None):
    output = body.get('max_completion_tokens',1024)
    body, tokens = fit_context(body, limits[2], output)
    quota = Quota(directory,key,body['model'],limits,status)
    opener = build_opener(proxy)
    req = urllib.request.Request('https://api.groq.com/openai/v1/chat/completions',
        data=json.dumps(body,ensure_ascii=False).encode(),
        headers={'Content-Type':'application/json','User-Agent':'EduCode/0.2 (Groq client)','Authorization':'Bearer '+key})
    for attempt in range(3):
        ticket = quota.reserve(tokens)
        try:
            with opener.open(req,timeout=120) as response:
                result = json.load(response)
                quota.update(ticket,response.headers,result.get('usage',{}).get('total_tokens'))
                return result['choices'][0]['message']
        except urllib.error.HTTPError as error:
            if error.code != 429: raise
            delay = max(2.1,duration(error.headers.get('retry-after')) or 60)
            quota.update(ticket,error.headers,cooldown=delay)
            if attempt == 2: raise RuntimeError('Groq пока ограничивает запросы. Нажмите «Повторить запрос» позже.') from None
            status('Groq ограничил запросы; ожидаю разрешённое время повторения…')
    raise RuntimeError('Groq не ответил')
