// Actual Qt WebEngine integration tests; CDP listens on localhost only during this test.
const {spawn,spawnSync}=require('child_process');
const fs=require('fs'),path=require('path'),assert=require('assert');
const root=path.resolve(__dirname,'..'),linux=process.platform==='linux',bundle=process.env.EDUCODE_TEST_BUNDLE||path.join(root,'out/build',linux?'linux-release':'educode-qt5-release','EduCode');
const project=path.join(root,'.runtime/smoke-project'),file=path.join(project,'main.py');
fs.mkdirSync(project,{recursive:true});
const source="import math\n\ndef greet(name: str) -> str:\n    return f'Привет, {name}!'\n\nname = input('Имя: ')\nprint(greet(name))\nprint(math.sqrt(16))\n";
fs.writeFileSync(file,source);fs.writeFileSync(path.join(project,'second.py'),'value = 42\n');
const floodFile=path.join(project,'output.py');fs.writeFileSync(floodFile,"for i in range(100000):\n    print(f'{i}: console output performance check')\nprint('FLOOD_DONE')\n");
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
for(const relative of ['math-notes.py','index-test/new_script.py']){const p=path.join(project,relative);if(fs.existsSync(p))fs.unlinkSync(p);}
const initialFolder=path.join(project,'index-test');if(fs.existsSync(initialFolder)&&fs.readdirSync(initialFolder).length===0)fs.rmdirSync(initialFolder);
const app=spawn(path.join(bundle,linux?'EduCode':'EduCode.exe'),[project],{windowsHide:true,env:{...process.env,EDUCODE_TEST_MODE:'1',QT_OPENGL:'software',QTWEBENGINE_REMOTE_DEBUGGING:'9237',EDUCODE_LOG:path.join(root,'out/settings-ui.log'),EDUCODE_SCREENSHOT:path.join(root,'out/verified-settings-final.png'),EDUCODE_SCREENSHOT_DELAY:'120000'}});
let exited=false;const exit=new Promise(resolve=>app.on('exit',code=>{exited=true;resolve(code);}));
class Client {
 constructor(url){const Socket=global.WebSocket||require('undici').WebSocket;this.ws=new Socket(url);this.seq=0;this.pending=new Map();this.connected=new Promise((res,rej)=>{this.ws.onopen=res;this.ws.onerror=rej;});this.ws.onmessage=e=>{const msg=JSON.parse(e.data);if(this.pending.has(msg.id)){const {res,rej}=this.pending.get(msg.id);this.pending.delete(msg.id);msg.error?rej(new Error(JSON.stringify(msg.error))):res(msg.result);}};}
 async command(method,params={}){await this.connected;return new Promise((res,rej)=>{const id=++this.seq;const timer=setTimeout(()=>rej(new Error('CDP timeout: '+method)),10000);this.pending.set(id,{res:v=>{clearTimeout(timer);res(v);},rej:e=>{clearTimeout(timer);rej(e);}});this.ws.send(JSON.stringify({id,method,params}));});}
 async eval(expression){const r=await this.command('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});if(r.exceptionDetails)throw new Error(JSON.stringify(r.exceptionDetails));return r.result.value;}
 async wait(expression){for(let i=0;i<50;i++){if(await this.eval(expression))return;await sleep(150);}throw new Error('Timed out: '+expression);}
 close(){this.ws.close();}
}
async function pages(){return (await fetch('http://127.0.0.1:9237/json/list')).json();}
async function connect(part){for(let i=0;i<50;i++){try{const p=(await pages()).find(p=>p.url.includes(part)&&!(part==='terminal.html'&&p.url.includes('?console')));if(p)return new Client(p.webSocketDebuggerUrl);}catch{}await sleep(150);}throw new Error('Page not found: '+part);}
const textBuffer='Array.from({length:term.buffer.active.length},(_,i)=>term.buffer.active.getLine(i).translateToString().trimEnd()).join("\\n")';

async function main(){
 const editor=await connect('editor.html');
 await editor.wait('typeof host!=="undefined"&&host&&editor&&!!editor.getModel()');
 await editor.eval('host.resetSettings();host.clearNotifications();true');
 await editor.wait('host.configuration["editor.fontSize"]===14');
 await editor.eval('host.openFile('+JSON.stringify(file)+');true');await editor.wait('host.activePath.endsWith("main.py")');
 const configPath=await editor.eval('host.configPath'), logPath=await editor.eval('host.logPath');
 await editor.eval('host.setSetting("editor.fontSize",18);host.setSetting("editor.wordWrap",true);host.setSetting("terminal.fontSize",17);true');
 await editor.wait('editor.getOption(monaco.editor.EditorOption.fontSize)===18');
 assert.equal(JSON.parse(fs.readFileSync(configPath))['editor.fontSize'],18);
 const consolePage=await connect('terminal.html?console');await consolePage.wait('typeof term!=="undefined"&&term.options.fontSize===17');
 const external=JSON.parse(fs.readFileSync(configPath));external['editor.fontSize']=16;fs.writeFileSync(configPath,JSON.stringify(external,null,2));
 await editor.wait('editor.getOption(monaco.editor.EditorOption.fontSize)===16');
 console.log('PASS live JSON/editor/terminal settings');
 await editor.eval('host.setSetting("ai.provider","gemini");host.setSetting("ai.geminiApiKey","offline-test-key");host.setSetting("ai.geminiModel","test-model");true');
 await editor.wait('host.configuration["ai.provider"]==="gemini"&&host.configuration["ai.geminiModel"]==="test-model"');
 assert.equal(JSON.parse(fs.readFileSync(configPath))['ai.geminiApiKey'],'offline-test-key');
 await editor.eval('host.setSetting("ai.geminiApiKey", "");host.setSetting("ai.provider","ollama");true');
 console.log('PASS provider settings persistence');

 await editor.eval('editor.getModel().setValue('+JSON.stringify("# search marker\nvalue = 'EDUCODE_CONTENT_726'\n")+');true');
 await sleep(250);await editor.eval('host.searchQuery("EDUCODE_CONTENT_726");true');
 await editor.wait('host.searchResults.some(r=>r.group==="Код проекта"&&r.line===2)');
 const hit=await editor.eval('host.searchResults.find(r=>r.group==="Код проекта"&&r.line===2)');
 await editor.eval('host.openFile('+JSON.stringify(hit.path)+',2,'+hit.column+');true');await editor.wait('editor.getPosition().lineNumber===2');
 console.log('PASS unsaved code content search and line navigation');
 const menu=await editor.eval('editor.getContribution("editor.contrib.contextmenu")._getMenuActions(editor.getModel(),editor.getPosition()).map(a=>a.id)');
 assert(!menu.includes('editor.action.quickCommand'));assert(await editor.eval('host.shortcuts.palette==="Ctrl+Shift+P"'));
 await editor.eval('host.command("palette");true');await editor.wait('!!document.querySelector(".quick-input-widget")&&document.querySelector(".quick-input-widget").style.display!=="none"');
 await editor.eval('document.querySelector(".quick-input-widget input").dispatchEvent(new KeyboardEvent("keydown",{key:"Escape",code:"Escape",keyCode:27,which:27,bubbles:true}));true');
 await editor.eval('host.setHotkey("find","Ctrl+Shift+F");true');await editor.wait('host.shortcuts.find==="Ctrl+Shift+F"');
 assert.equal(await editor.eval('!!editor._standaloneKeybindingService.lookupKeybinding("actions.find")'),false);
 console.log('PASS palette removed from menu; available command and configurable hotkeys');
 const width=await editor.eval('innerWidth');
 await editor.eval('host.notify("Проверка", "Уведомление внутри IDE");host.command("notifications");true');await editor.wait('innerWidth<'+(width-300));
 await editor.eval('host.command('+JSON.stringify('test-ui-shot '+path.join(root,'out/verified-notifications.png'))+');true');
 const endpointPath=fs.readdirSync(path.dirname(configPath)).find(f=>f==='control-'+app.pid+'.json');assert(endpointPath);
 const endpoint=JSON.parse(fs.readFileSync(path.join(path.dirname(configPath),endpointPath)));
 const request=(method,params,token=endpoint.token)=>new Promise((resolve,reject)=>{const client=require('net').connect(endpoint.port,'127.0.0.1',()=>client.write(JSON.stringify({token,method,params})+'\n'));let data='';client.on('data',b=>data+=b);client.on('error',reject);client.on('end',()=>resolve(JSON.parse(data)));});
 assert.equal((await request('config.get',{})).ok,true);assert.equal((await request('notify',{title:'wrong'},'invalid')).ok,false);
 assert.equal((await request('config.patch',{'editor.fontSize':200})).ok,false);
 assert.equal((await request('notify',{title:'IPC',message:'Local control API'})).ok,true);
 const apiFile=path.join(project,'notify-test.py');fs.writeFileSync(apiFile,'import educode\neducode.notify("Python API", "Вызов из программы")\nprint("API_DONE")\n');
 await editor.eval('host.openFile('+JSON.stringify(apiFile)+');true');await editor.wait('host.activePath.replace(/\\\\/g,"/").endsWith("notify-test.py")');
 await editor.eval('host.setRunTarget(host.activePath);host.run();true');await editor.wait('host.notifications.some(n=>n.title==="Python API")');await editor.wait('!host.running');
 await editor.eval('host.command("terminal");true');const terminal=await connect('terminal.html');await terminal.wait('typeof host!=="undefined"&&host');
 await terminal.eval('host.terminalInput("python -m educode notify \\\"Terminal API\\\" \\\"Message\\\"\\r");true');
 await editor.wait('host.notifications.some(n=>n.title==="Terminal API")');
 console.log('PASS notifications from IDE/Python/terminal and authenticated local API');
 const count=await editor.eval('host.notifications.length');await editor.eval('host.setSetting("notifications.delivery","system");host.notify("EduCode system test","Системное уведомление");true');await sleep(300);
 assert.equal(await editor.eval('host.notifications.length'),count);
 await editor.eval('host.setSetting("notifications.delivery","both");host.notify("EduCode both test","IDE и система");true');await editor.wait('host.notifications.length==='+ (count+1));
 console.log('PASS system and both notification routing');
 await editor.eval('host.command("browser");true');
 let browser;for(let i=0;i<100;i++){const page=(await pages()).find(p=>p.url.includes('google.com'));if(page){browser=new Client(page.webSocketDebuggerUrl);break;}await sleep(150);}assert(browser,'Google browser created');
 await browser.wait('document.title.length>0');assert.equal(await browser.eval('typeof qt'), 'undefined');
 await editor.eval('host.command('+JSON.stringify('test-ui-shot '+path.join(root,'out/verified-browser.png'))+');true');
 console.log('PASS Google mini browser isolated from IDE WebChannel');
 const server=require('http').createServer((req,res)=>{res.writeHead(200,{'Content-Type':'text/html;charset=utf-8'});res.end('<title>EduCode Docs</title><h2>Python documentation test</h2><p>Browser stays alongside code.</p>');});await new Promise(r=>server.listen(0,'127.0.0.1',r));
 await editor.eval('host.setSetting("browser.homePage",'+JSON.stringify('http://127.0.0.1:'+server.address().port)+');true');await browser.wait('document.title==="EduCode Docs"');browser.close();server.close();
 console.log('PASS configurable browser home page');
 const proof=path.join(project,'system-console.json'),nativeFile=path.join(project,'system-test.py');if(fs.existsSync(proof))fs.unlinkSync(proof);
 if(!linux){
  fs.writeFileSync(nativeFile,'import ctypes, json, sys, educode\nfrom pathlib import Path\nPath('+JSON.stringify(proof.replace(/\\/g,'/'))+').write_text(json.dumps({"console": ctypes.windll.kernel32.GetConsoleWindow(), "tty":sys.stdout.isatty()}))\nprint("NATIVE_ONLY_OUTPUT")\neducode.notify("System console API", "Works")\n');
 await editor.eval('host.setSetting("notifications.delivery","ide");host.setSetting("console.output","system");host.openFile('+JSON.stringify(nativeFile)+');true');await editor.wait('host.activePath.endsWith("system-test.py")');
 const before=await consolePage.eval(textBuffer);await editor.eval('host.setRunTarget(host.activePath);host.run();true');
 for(let i=0;i<50&&!fs.existsSync(proof);i++)await sleep(150);assert(fs.existsSync(proof));const native=JSON.parse(fs.readFileSync(proof));assert(native.console!==0&&native.tty);
 await editor.wait('host.notifications.some(n=>n.title==="System console API")');assert.equal(await consolePage.eval(textBuffer),before);await editor.eval('host.stop();true');await editor.wait('!host.running');
 console.log('PASS real Windows console and no output duplicated to IDE');
 } else console.log('SKIP Windows-only system console assertion on Linux');
 await editor.eval('host.resetSettings();host.openFile('+JSON.stringify(file)+');true');await editor.wait('host.activePath.endsWith("main.py")');
 await editor.eval('host.command("test-ui-click 1258 172 left");true');await sleep(250);
 await editor.eval('host.command('+JSON.stringify('test-ui-shot '+path.join(root,'out/verified-ai-empty.png'))+');true');
 await editor.eval('host.command("test-ui-click 1246 28 left");true');await sleep(250);
 await editor.eval('host.command('+JSON.stringify('test-ui-shot '+path.join(root,'out/verified-settings.png'))+');true');
 await editor.eval('host.command("test-ui-click 100 510 left");true');await sleep(200);
 await editor.eval('host.command('+JSON.stringify('test-ui-shot '+path.join(root,'out/verified-settings-about.png'))+');true');
 assert(fs.existsSync(logPath));assert(fs.readFileSync(logPath,'utf8').includes('NOTIFICATION'));console.log('PASS persistent system log');
 for(const f of [apiFile,nativeFile,proof])if(fs.existsSync(f))fs.unlinkSync(f);
 await editor.eval('host.openFile('+JSON.stringify(file)+');true');await editor.eval('editor.getModel().setValue('+JSON.stringify(source)+');host.save();true');
 editor.close();terminal.close();consolePage.close();app.kill();await exit;
}
main().then(()=>console.log('PASS settings/sidebar/API integration')).catch(e=>{console.error(e);if(!exited)app.kill();process.exitCode=1;});


