"""Check actual desktop environment and menu launcher, without test mode or forced X11."""
import os, subprocess, time
from pathlib import Path
root=Path(__file__).resolve().parents[1]
env=dict(os.environ)
for line in subprocess.check_output(['systemctl','--user','show-environment'],text=True).splitlines():
    key,_,value=line.partition('=')
    if key in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','XDG_SESSION_TYPE','QT_IM_MODULE','QT_ACCESSIBILITY'): env[key]=value
for key in ('QT_QPA_PLATFORM','EDUCODE_TEST_MODE'): env.pop(key,None)
env['XDG_RUNTIME_DIR']='/run/user/'+str(os.getuid())
env['EDUCODE_SCREENSHOT_DELAY']='6000'
for name,command in [('terminal',['EduCode']),('menu',['gio','launch',str(Path.home()/'.local/share/applications/EduCode.desktop')])]:
    shot=root/'out'/('linux-launch-'+name+'.png')
    if shot.exists(): shot.unlink()
    env['EDUCODE_SCREENSHOT']=str(shot)
    with (root/'out'/('linux-launch-'+name+'.txt')).open('w') as log:
        result=subprocess.run(command,env=env,stdout=log,stderr=log,timeout=20)
    assert result.returncode==0,(name,result.returncode)
    end=time.monotonic()+15
    while not shot.exists() and time.monotonic()<end: time.sleep(.2)
    assert shot.exists() and shot.stat().st_size>10000,name+' did not show a window'
    print('PASS '+name+' launch in '+env.get('XDG_SESSION_TYPE','unknown')+' desktop',flush=True)
