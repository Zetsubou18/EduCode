"""Linux desktop integration: CLI file, Python input, PTY, API and screenshot."""
import json, os, shutil, socket, subprocess, tempfile, time
from pathlib import Path
root=Path(__file__).resolve().parents[1]
binary=Path(shutil.which('EduCode') or root/'out/build/linux-release/EduCode/EduCode')
assert subprocess.check_output([str(binary),'--version'],text=True).strip()=='EduCode 0.3.0'
assert 'Usage:' in subprocess.check_output([str(binary),'--help'],text=True)
assert subprocess.run([str(binary),'/definitely/missing/educode.py'],capture_output=True).returncode==2
print('PASS CLI help/version/missing path',flush=True)
env=dict(os.environ,EDUCODE_TEST_MODE='1',EDUCODE_SCREENSHOT=str(root/'out/linux-ui.png'),EDUCODE_SCREENSHOT_DELAY='45000')
for line in subprocess.check_output(['systemctl','--user','show-environment'],text=True).splitlines():
    key,_,value=line.partition('=')
    if key in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','XDG_SESSION_TYPE','QT_IM_MODULE','QT_ACCESSIBILITY'): env[key]=value
env['XDG_RUNTIME_DIR']='/run/user/'+str(os.getuid())
env.pop('QT_QPA_PLATFORM',None)
with tempfile.TemporaryDirectory(prefix='educode-linux-') as tmp:
    project=Path(tmp);(project/'src').mkdir()
    subprocess.run(['python3','-m','venv',str(project/'.venv')],check=True)
    code="print('RUN_OK',flush=True)\nvalue=input('INPUT:')\nprint('INPUT_OK:'+value,flush=True)\n"
    (project/'src/test.py').write_text(code,encoding='utf-8')
    (project/'src/second.py').write_text('value = 42\n',encoding='utf-8')
    with (root/'out/linux-ui-stderr.log').open('w') as log:
        app=subprocess.Popen([str(binary),'test.py','second.py'],cwd=project/'src',env=env,stdout=log,stderr=log)
        endpoint=Path.home()/'.config/EduCodeTests/EduCode'/('control-'+str(app.pid)+'.json')
        def request(method,params=None):
            data=json.loads(endpoint.read_text())
            with socket.create_connection(('127.0.0.1',data['port']),timeout=5) as connection:
                connection.sendall(json.dumps(dict(token=data['token'],method=method,params=params or {})).encode()+b'\n')
                response=json.loads(connection.makefile('rb').readline(4*1024*1024))
            assert response.get('ok'), response
            return response.get('result')
        def wait(check,seconds=20):
            end=time.monotonic()+seconds
            while time.monotonic()<end:
                assert app.poll() is None, 'IDE exited: '+str(app.returncode)
                try:
                    result=check()
                    if result: return result
                except (OSError,ValueError): pass
                time.sleep(.2)
            raise AssertionError('Timed out')
        try:
            context=wait(lambda: endpoint.exists() and request('ai.context'))
            assert context['project']==str(project), context['project']
            assert context['activeFile']==str(project/'src/second.py'),context['activeFile']
            request('command',{'name':'close'})
            wait(lambda: request('ai.context')['activeFile']==str(project/'src/test.py'))
            wait(lambda: 'editorReady' in request('ai.context')['log'])
            print('PASS relative file paths, multiple files, ancestor project',flush=True)
            request('command',{'name':'run'})
            wait(lambda: 'INPUT:' in request('ai.context')['console'])
            request('console.input',{'text':'linux-test\n'})
            wait(lambda: 'INPUT_OK:linux-test' in request('ai.context')['console'])
            print('PASS Python run and interactive input',flush=True)
            request('project.write',{'path':'src/test.py','content':'print(missing_linux_symbol)\n'})
            wait(lambda: len(request('ai.context')['problems'])>0)
            request('project.write',{'path':'src/test.py','content':code})
            time.sleep(2)
            wait(lambda: not request('ai.context')['problems'] and request('ai.context')['activeCode']==code)
            print('PASS Pyright diagnostics and updated code',flush=True)
            request('command',{'name':'terminal'})
            time.sleep(3)
            request('terminal.input',{'text':"python -c \"import sys; print('PTY_PYTHON='+sys.executable)\"\r"})
            wait(lambda: 'PTY_PYTHON='+str(project/'.venv/bin/python') in request('ai.context')['terminal'])
            print('PASS Linux PTY and project venv',flush=True)
            request('notify',{'title':'Linux check','message':'API_OK'})
            print('PASS local API notification',flush=True)
            app.wait(timeout=50)
            assert app.returncode==0,app.returncode
            assert (root/'out/linux-ui.png').stat().st_size>10000
            print('PASS graphical startup, screenshot, clean shutdown',flush=True)
        finally:
            if app.poll() is None: app.terminate();app.wait(timeout=10)

# A standalone file must open even before a project environment is created.
with tempfile.TemporaryDirectory(prefix='educode-file-') as tmp:
    file=Path(tmp)/'пример программы.py'
    file.write_text("print('Standalone file')\n",encoding='utf-8')
    env['EDUCODE_SCREENSHOT']=str(root/'out/linux-standalone.png')
    env['EDUCODE_SCREENSHOT_DELAY']='5000'
    with (root/'out/linux-standalone-stderr.log').open('w') as log:
        app=subprocess.Popen([str(binary),file.name],cwd=tmp,env=env,stdout=log,stderr=log)
        endpoint=Path.home()/'.config/EduCodeTests/EduCode'/('control-'+str(app.pid)+'.json')
        try:
            context=wait(lambda: endpoint.exists() and request('ai.context'),seconds=4)
            assert context['activeFile']==str(file),context['activeFile']
            assert context['activeCode']==file.read_text(encoding='utf-8')
            assert not (Path(tmp)/'.venv').exists()
            app.wait(timeout=10)
            assert app.returncode==0,app.returncode
            print('PASS standalone file, Unicode/spaces, no forced venv creation',flush=True)
        finally:
            if app.poll() is None: app.terminate();app.wait(timeout=10)
