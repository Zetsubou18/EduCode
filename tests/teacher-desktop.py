"""Desktop integration helper for Teacher Mode, using the test-only local control API."""
import json,os,socket,subprocess,sys,time
from pathlib import Path
root=Path(__file__).resolve().parents[1]
state=root/'out/teacher-desktop-state.json'
def request(endpoint,method,params):
 data=json.loads(Path(endpoint).read_text())
 with socket.create_connection(('127.0.0.1',data['port']),timeout=10) as conn:
  conn.sendall(json.dumps(dict(token=data['token'],method=method,params=params)).encode()+b'\n')
  result=json.loads(conn.makefile('rb').readline(24*1024*1024))
 assert result['ok'],result
 return result.get('result')
if sys.argv[1]=='start':
 project=root/'out/teacher-desktop-project';project.mkdir(exist_ok=True)
 if not (project/'.venv').exists():subprocess.run([sys.executable,'-m','venv',str(project/'.venv')],check=True)
 file=project/'main.py';file.write_text("import tkinter as tk\nprint('DEMO_CONSOLE_OK', flush=True)\nroot = tk.Tk()\nroot.title('Teacher Mode test UI')\nroot.geometry('360x220')\nroot.configure(bg='#00cc88')\ntk.Label(root, text='ALLOWED PYTHON WINDOW', bg='#00cc88', font=('Arial', 18)).pack(pady=55)\nroot.mainloop()\n",encoding='utf-8')
 env=dict(os.environ,EDUCODE_TEST_MODE='1',EDUCODE_SCREENSHOT=str(root/'out/teacher-desktop.png'),EDUCODE_SCREENSHOT_DELAY='240000')
 if os.name!='nt':
  for line in subprocess.check_output(['systemctl','--user','show-environment'],text=True).splitlines():
   key,_,value=line.partition('=')
   if key in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','XDG_SESSION_TYPE','QT_IM_MODULE'):env[key]=value
  env['XDG_RUNTIME_DIR']='/run/user/'+str(os.getuid());env.pop('QT_QPA_PLATFORM',None)
  binary=root/'out/build/linux-release/EduCode/EduCode'
 else:binary=root/'out/build/educode-qt5-release/EduCode/EduCode.exe'
 with (root/'out/teacher-desktop.log').open('w') as log:
  app=subprocess.Popen([str(binary),str(file)],env=env,stdout=log,stderr=log,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
 endpoint=(Path(os.environ['LOCALAPPDATA'])/'EduCodeTests/EduCode' if os.name=='nt' else Path.home()/'.config/EduCodeTests/EduCode')/('control-'+str(app.pid)+'.json')
 state.write_text(json.dumps(dict(pid=app.pid,endpoint=str(endpoint))))
 for _ in range(100):
  if endpoint.exists():print(json.dumps(dict(pid=app.pid,endpoint=str(endpoint))),flush=True);break
  time.sleep(.1)
 else:raise AssertionError('No desktop control endpoint')
elif sys.argv[1]=='request':
 print(json.dumps(request(json.loads(state.read_text())['endpoint'],sys.argv[2],json.loads(sys.argv[3]) if len(sys.argv)>3 else {})))
elif sys.argv[1]=='stop':
 pid=json.loads(state.read_text())['pid']
 if os.name=='nt':subprocess.run(['taskkill','/PID',str(pid),'/T','/F'],capture_output=True)
 else:os.kill(pid,15)
