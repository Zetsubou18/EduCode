"""Restricted Ollama tool runner owned by EduCode. The model only sees declared IDE tools."""
from ai_http import groq_request, build_opener
import urllib.error
import html, ipaddress, json, os, re, socket, sys, urllib.parse, urllib.request

def report_error(kind, error, traceback):
    message=str(error)
    key=os.environ.get('EDUCODE_GROQ_API_KEY','')
    if key: message=message.replace(key,'[скрыто]')
    print(message[:1500], file=sys.stderr, flush=True)

sys.excepthook=report_error
request = json.load(open(sys.argv[1], encoding='utf-8'))
endpoint = request['endpoint']
control = json.load(open(endpoint, encoding='utf-8'))

def emit(kind, **data):
    print(json.dumps({'type': kind, **data}, ensure_ascii=False), flush=True)

def ide(method, params=None):
    payload = {'token': control['token'], 'method': method, 'params': params or {}}
    with socket.create_connection(('127.0.0.1', control['port']), timeout=15) as client:
        client.sendall(json.dumps(payload, ensure_ascii=False).encode() + b'\n')
        raw = client.makefile('rb').readline(4 * 1024 * 1024)
    key = os.environ.get('EDUCODE_GROQ_API_KEY', '')
    if key: raw = raw.replace(key.encode(), b'[REDACTED]')
    reply = json.loads(raw)
    if not reply.get('ok'): raise RuntimeError(reply.get('error') or 'IDE отказала в операции')
    return reply.get('result', True)

def web_search(query):
    emit('status', text='Ищу в интернете…')
    url = 'https://html.duckduckgo.com/html/?q=' + urllib.parse.quote(query)
    req = urllib.request.Request(url, headers={'User-Agent':'Mozilla/5.0 EduCode/0.1'})
    text = build_opener(request.get('proxy','')).open(req, timeout=15).read().decode('utf-8','replace')
    found=[]
    for link,title in re.findall(r'class="result__a"[^>]*href="([^"]+)"[^>]*>(.*?)</a>', text):
        link=html.unescape(link); match=re.search(r'uddg=([^&]+)',link)
        found.append({'title':re.sub('<[^>]+>','',html.unescape(title)), 'url':urllib.parse.unquote(match.group(1)) if match else link})
        if len(found)>=6: break
    emit('status', text='Найдено страниц: '+str(len(found)))
    return found

def web_fetch(url):
    if not url.startswith(('http://','https://')): raise RuntimeError('Разрешены только HTTP(S)-страницы.')
    host=urllib.parse.urlparse(url).hostname
    if not host: raise RuntimeError('Некорректный адрес.')
    for answer in socket.getaddrinfo(host,None):
        address=ipaddress.ip_address(answer[4][0])
        if address.is_private or address.is_loopback or address.is_link_local or address.is_reserved:
            raise RuntimeError('Локальные и служебные адреса недоступны веб-инструменту.')
    emit('status', text='Читаю страницу…')
    req=urllib.request.Request(url,headers={'User-Agent':'Mozilla/5.0 EduCode/0.1'})
    raw=build_opener(request.get('proxy','')).open(req,timeout=15).read(600000).decode('utf-8','replace')
    raw=re.sub(r'(?is)<(script|style).*?>.*?</\1>',' ',raw)
    return re.sub(r'\s+',' ',re.sub(r'<[^>]+>',' ',html.unescape(raw)))[:18000]

tool_specs=[
 {'type':'function','function':{'name':'ide_context','description':'Получить текущий контекст IDE: проект, активный код, проблемы, консоль, пакеты, время и ОС.','parameters':{'type':'object','properties':{}}}},
 {'type':'function','function':{'name':'project_list','description':'Список файлов пользовательского проекта.','parameters':{'type':'object','properties':{}}}},
 {'type':'function','function':{'name':'project_read','description':'Прочитать UTF-8 файл проекта.','parameters':{'type':'object','properties':{'path':{'type':'string'}},'required':['path']}}},
 {'type':'function','function':{'name':'project_write','description':'Создать или полностью заменить UTF-8 файл проекта.','parameters':{'type':'object','properties':{'path':{'type':'string'},'content':{'type':'string'}},'required':['path','content']}}},
 {'type':'function','function':{'name':'project_delete','description':'Удалить файл проекта в корзину.','parameters':{'type':'object','properties':{'path':{'type':'string'}},'required':['path']}}},
 {'type':'function','function':{'name':'settings_patch','description':'Изменить настройки IDE по ключам JSON-конфигурации.','parameters':{'type':'object','properties':{'changes':{'type':'object'}},'required':['changes']}}},
 {'type':'function','function':{'name':'console_input','description':'Ввести текст в запущенную Python-консоль.','parameters':{'type':'object','properties':{'text':{'type':'string'}},'required':['text']}}},
 {'type':'function','function':{'name':'terminal_input','description':'Ввести команду в терминал проекта.','parameters':{'type':'object','properties':{'text':{'type':'string'}},'required':['text']}}},
 {'type':'function','function':{'name':'package_action','description':'Установить, обновить или удалить пакет активного venv.','parameters':{'type':'object','properties':{'action':{'type':'string','enum':['install','update','remove']},'name':{'type':'string'}},'required':['action','name']}}},
 {'type':'function','function':{'name':'notify','description':'Отправить уведомление по правилам IDE.','parameters':{'type':'object','properties':{'title':{'type':'string'},'message':{'type':'string'}},'required':['title','message']}}},
 {'type':'function','function':{'name':'browser_open','description':'Открыть HTTP(S) страницу в боковом браузере EduCode.','parameters':{'type':'object','properties':{'url':{'type':'string'}},'required':['url']}}},
 {'type':'function','function':{'name':'web_search','description':'Поиск свежей информации в интернете.','parameters':{'type':'object','properties':{'query':{'type':'string'}},'required':['query']}}},
 {'type':'function','function':{'name':'web_fetch','description':'Прочитать текст найденной HTTP(S) страницы.','parameters':{'type':'object','properties':{'url':{'type':'string'}},'required':['url']}}},
 {'type':'function','function':{'name':'memory_update','description':'Запомнить или удалить устойчивый факт. kind=user — о пользователе, kind=facts — рабочий факт. value пустой для удаления ключа.','parameters':{'type':'object','properties':{'kind':{'type':'string','enum':['user','facts']},'key':{'type':'string'},'value':{'type':'string'}},'required':['kind','key','value']}}},
]

def execute(name,args):
    labels={'ide_context':'Читаю контекст IDE…','project_list':'Изучаю структуру проекта…','project_read':'Читаю код…','project_write':'Редактирую код…','project_delete':'Удаляю файл проекта…','settings_patch':'Меняю настройки IDE…','console_input':'Ввожу данные в консоль…','terminal_input':'Работаю с терминалом…','package_action':'Управляю библиотеками…','notify':'Отправляю уведомление…','browser_open':'Открываю документацию…','memory_update':'Обновляю память…'}
    emit('status',text=labels.get(name,'Выполняю действие…'))
    if name=='web_search': return web_search(args['query'])
    if name=='web_fetch': return web_fetch(args['url'])
    mapping={'ide_context':'ai.context','project_list':'project.list','project_read':'project.read','project_write':'project.write','project_delete':'project.delete','settings_patch':'config.patch','console_input':'console.input','terminal_input':'terminal.input','package_action':'package.action','notify':'notify','browser_open':'browser.open','memory_update':'memory.update'}
    params=args.get('changes',{}) if name=='settings_patch' else args
    return ide(mapping[name],params)

base=request.get('url','http://127.0.0.1:11434').rstrip('/')
def ollama(messages, tools=None):
    groq = request.get('provider') == 'groq'
    body={'model':request['model'],'messages':messages,'stream':False}
    headers={'Content-Type':'application/json','User-Agent':'EduCode/0.2 (Groq client)'}
    if tools: body['tools']=tools
    if groq:
        body['max_completion_tokens']=request.get('outputTokens',1024)
        try:
            return groq_request(body,os.environ.get('EDUCODE_GROQ_API_KEY',''),request.get('proxy',''),
                request['rateStateDir'],tuple(request.get('rateLimits',[30,1000,8000,200000])),
                lambda text: emit('status',text=text))
        except urllib.error.HTTPError as error:
            raw=error.read(8192).decode('utf-8','replace')
            try: detail=json.loads(raw).get('error',{}).get('message','')
            except (ValueError, AttributeError): detail=''
            if '1010' in raw: detail='Сервис заблокировал HTTP-клиент. Проверьте версию EduCode.'
            key=os.environ.get('EDUCODE_GROQ_API_KEY','')
            if key: detail=detail.replace(key,'[скрыто]')
            raise RuntimeError('ИИ HTTP '+str(error.code)+': '+(detail[:1000] or 'Проверьте ключ, модель и доступ к сервису.')) from None
        except (urllib.error.URLError, OSError) as error:
            raise RuntimeError('Нет соединения с Groq. Проверьте прокси/VPN и доступ к сети.') from None
    body['options']={'temperature':0.2,'num_ctx':request.get('context',8192)}
    url=base+'/api/chat'
    req=urllib.request.Request(url,data=json.dumps(body,ensure_ascii=False).encode(),headers=headers)
    try:
        with urllib.request.urlopen(req,timeout=120 if groq else 600) as response:
            result=json.load(response)
    except urllib.error.HTTPError as error:
        if error.code == 429:
            raise RuntimeError('Лимит Groq. Повторите через '+error.headers.get('retry-after','несколько')+' секунд.') from None
        raw=error.read(8192).decode('utf-8','replace')
        try: detail=json.loads(raw).get('error',{}).get('message','')
        except (ValueError, AttributeError): detail=''
        if '1010' in raw: detail='Сервис заблокировал HTTP-клиент. Проверьте версию EduCode.'
        key=os.environ.get('EDUCODE_GROQ_API_KEY','')
        if key: detail=detail.replace(key,'[скрыто]')
        raise RuntimeError('ИИ HTTP '+str(error.code)+': '+(detail[:1000] or 'Проверьте ключ, модель и доступ к сервису.')) from None
    return result['choices'][0]['message'] if groq else result['message']

history=request.get('messages',[])
limit=request.get('memoryLimit',24000)
summary=request.get('summary','')
if sum(len(m.get('content','')) for m in history)>limit and len(history)>6:
    emit('status',text='Сжимаю память этого чата…')
    prompt=[{'role':'system','content':'Кратко сожми историю диалога: сохрани требования, решения, изменённые файлы и незавершённые задачи.'},{'role':'user','content':json.dumps(history[:-6],ensure_ascii=False)}]
    prompt[1]['content'] = summary + '\n' + prompt[1]['content']
    summary=ollama(prompt).get('content','')[:10000]
    history=history[-6:]
    emit('summary',text=summary)

system=request['system']+'\nПамять чата: '+summary+'\nПамять о пользователе: '+json.dumps(request.get('userMemory',{}),ensure_ascii=False)+'\nРабочие факты: '+json.dumps(request.get('factMemory',{}),ensure_ascii=False)+'\nПользовательские инструкции: '+request.get('userPrompt','')
messages=[{'role':'system','content':system}]+[{'role':m['role'],'content':m.get('content','')} for m in history]+[{'role':'user','content':request['prompt']}]
emit('status',text='Думаю…')
needs_tools=bool(re.search(r'уведом|notify|ошиб|консол|терминал|файл|код|проект|настрой|пакет|библиот|установ|удал|браузер|открой|найди|интернет|документ',request['prompt'],re.I))
used_tools=False
corrected=False
for step in range(12):
    answer=ollama(messages,tool_specs)
    messages.append(answer)
    calls=answer.get('tool_calls') or []
    if not calls:
        if needs_tools and not used_tools and not corrected:
            messages.append({'role':'user','content':'Ты описал действие без результата инструмента. Не утверждай, что действие выполнено. Сейчас вызови подходящий инструмент IDE, проверь его результат и только потом ответь.'})
            corrected=True
            emit('status',text='Проверяю действие через IDE…')
            continue
        emit('answer',text=answer.get('content','').strip() or 'Готово.')
        break
    for call in calls:
        used_tools=True
        fn=call['function']; name=fn['name']; args=fn.get('arguments') or {}
        if isinstance(args,str):
            try: args=json.loads(args)
            except ValueError: args={}
        try: result=execute(name,args)
        except Exception as error: result={'error':str(error)}
        emit('tool',name=name,arguments=args,result=result)
        message={'role':'tool','content':json.dumps(result,ensure_ascii=False)[:30000]}
        if request.get('provider') == 'groq': message['tool_call_id']=call['id']
        messages.append(message)
else: emit('answer',text='Достигнут лимит действий. Проверьте результат и продолжите новым сообщением.')
