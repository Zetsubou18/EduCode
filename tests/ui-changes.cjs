// Actual Qt WebEngine integration tests; CDP listens on localhost only during this test.
const {spawn,spawnSync}=require('child_process');
const fs=require('fs'),path=require('path'),assert=require('assert');
const root=path.resolve(__dirname,'..'),bundle=process.env.EDUCODE_TEST_BUNDLE||path.join(root,'out/build/educode-qt5-release/EduCode');
const project=path.join(root,'.runtime/smoke-project'),file=path.join(project,'main.py');
const source="import math\n\ndef greet(name: str) -> str:\n    return f'Привет, {name}!'\n\nname = input('Имя: ')\nprint(greet(name))\nprint(math.sqrt(16))\n";
fs.writeFileSync(file,source);fs.writeFileSync(path.join(project,'second.py'),'value = 42\n');
const floodFile=path.join(project,'output.py');fs.writeFileSync(floodFile,"for i in range(100000):\n    print(f'{i}: console output performance check')\nprint('FLOOD_DONE')\n");
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
for(const relative of ['math-notes.py','index-test/new_script.py']){const p=path.join(project,relative);if(fs.existsSync(p))fs.unlinkSync(p);}
const initialFolder=path.join(project,'index-test');if(fs.existsSync(initialFolder)&&fs.readdirSync(initialFolder).length===0)fs.rmdirSync(initialFolder);
const app=spawn(path.join(bundle,'EduCode.exe'),[project],{windowsHide:true,env:{...process.env,EDUCODE_TEST_MODE:'1',QT_OPENGL:'software',QTWEBENGINE_REMOTE_DEBUGGING:'9237',EDUCODE_LOG:path.join(root,'out/ui-changes.log'),EDUCODE_SCREENSHOT:path.join(root,'out/verified-changes.png'),EDUCODE_SCREENSHOT_DELAY:'30000'}});
let exited=false;const exit=new Promise(resolve=>app.on('exit',code=>{exited=true;resolve(code);}));
class Client {
 constructor(url){this.ws=new WebSocket(url);this.seq=0;this.pending=new Map();this.connected=new Promise((res,rej)=>{this.ws.onopen=res;this.ws.onerror=rej;});this.ws.onmessage=e=>{const msg=JSON.parse(e.data);if(this.pending.has(msg.id)){const {res,rej}=this.pending.get(msg.id);this.pending.delete(msg.id);msg.error?rej(new Error(JSON.stringify(msg.error))):res(msg.result);}};}
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
 await editor.wait('typeof editor!=="undefined"&&editor&&typeof host!=="undefined"&&host&&!!editor.getModel()&&innerWidth>100');
 await editor.wait('host.runTargets.some(t=>t.name==="second.py")');
 console.log('PASS unopened scripts in run selector');
 const folder=path.join(project,'index-test');fs.mkdirSync(folder,{recursive:true});await sleep(500);
 const newFile=path.join(folder,'new_script.py');fs.writeFileSync(newFile,'print(42)\n');
 await editor.wait('host.runTargets.some(t=>t.name==="index-test/new_script.py")');
 console.log('PASS live file creation in initially empty folder');
 await editor.eval('host.fileOperation("newFile",'+JSON.stringify(project)+',"math-notes.py");true');
 await editor.wait('host.runTargets.some(t=>t.name==="math-notes.py")');
 await editor.eval('host.searchQuery("math");true');
 await editor.wait('host.searchResults.some(r=>r.title==="math"&&r.group==="Библиотеки Python")');
 const results=await editor.eval('host.searchResults');assert.equal(results[0].group,'Файлы проекта');assert(results.some(r=>r.title==='math-notes.py'));console.log('PASS project-first search and compiled module stubs');
 await editor.eval('host.searchQuery("(12+8)/4");true');await editor.wait('host.searchResults.some(r=>r.value==="5")');
 await editor.eval('host.copyText("5");true');assert.equal(await editor.eval('new Promise(resolve=>host.clipboardText(resolve))'),'5');console.log('PASS calculator and clipboard');
 const actions=await editor.eval('editor.getSupportedActions().map(a=>({id:a.id,label:a.label}))');
 assert(actions.some(a=>/[А-Яа-я]/.test(a.label)));assert(actions.some(a=>a.id==='editor.action.quickCommand'));console.log('PASS Russian editor actions; Command Palette retained');
 const consolePage=await connect('terminal.html?console');await consolePage.wait('typeof host!=="undefined"&&host');
 await consolePage.eval('document.dispatchEvent(new MouseEvent("contextmenu",{clientX:30,clientY:30,bubbles:true}));true');
 assert((await consolePage.eval('menu.textContent')).includes('Копировать'));assert.equal(await consolePage.eval('menu.style.display'),'block');
 const consoleShot=await consolePage.command('Page.captureScreenshot',{format:'png'});fs.writeFileSync(path.join(root,'out/verified-console-menu.png'),Buffer.from(consoleShot.data,'base64'));await consolePage.eval('dismissMenu();true');console.log('PASS Russian console context menu');
 const width=await editor.eval('innerWidth');await editor.eval('host.command("test-ui-click 78 28 left");true');await sleep(400);await editor.eval('host.command("test-ui-inspect");true');await editor.eval('host.command('+JSON.stringify('test-ui-shot '+path.join(root,'out/toggle-debug.png'))+');true');await editor.wait('innerWidth>1200');
 await editor.eval('host.command("test-ui-click 78 28 left");true');await editor.wait('innerWidth<1100');console.log('PASS explorer hide/show through toolbar');
 const height=await consolePage.eval('innerHeight');
 await editor.eval('host.command("test-ui-drag 900 554 900 414");true');await consolePage.wait('innerHeight>'+height+'+90');
 await editor.eval('host.command("test-ui-drag 900 414 900 554");true');await sleep(250);console.log('PASS bottom panel resize by dragging divider');
 await editor.eval('host.command("test-ui-click 100 196 right");true');await sleep(200);
 await editor.eval('host.command('+JSON.stringify('test-ui-shot '+path.join(root,'out/verified-explorer-menu.png'))+');true');
 await editor.eval('host.command("test-ui-click 700 300 left");true');
 await editor.eval('host.command("test-ui-click 1202 28 left");true');await sleep(250);
 await editor.eval('host.command("test-ui-text math");true');await sleep(250);
 await editor.eval('host.command('+JSON.stringify('test-ui-shot '+path.join(root,'out/verified-search.png'))+');true');
 await editor.eval('host.command("test-ui-click 60 500 left");true');
 fs.unlinkSync(newFile);fs.rmdirSync(folder);fs.unlinkSync(path.join(project,'math-notes.py'));
 await editor.wait('!host.runTargets.some(t=>t.name==="math-notes.py")');console.log('PASS live deletion refresh');
 console.log('All change assertions passed.');
 const result=await Promise.race([exit,sleep(35000).then(()=>{app.kill();throw new Error('Screenshot timeout');})]);assert.equal(result,0);editor.close();consolePage.close();
}
main().then(()=>console.log('PASS requested changes')).catch(e=>{console.error(e);if(!exited)app.kill();process.exitCode=1;});

