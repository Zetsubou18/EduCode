// Actual Qt WebEngine integration tests; CDP listens on localhost only during this test.
const {spawn,spawnSync}=require('child_process');
const fs=require('fs'),path=require('path'),assert=require('assert');
const root=path.resolve(__dirname,'..'),bundle=process.env.EDUCODE_TEST_BUNDLE||path.join(root,'out/build/educode-qt5-release/EduCode');
const project=path.join(root,'.runtime/smoke-project'),file=path.join(project,'main.py');
const source="import math\n\ndef greet(name: str) -> str:\n    return f'Привет, {name}!'\n\nname = input('Имя: ')\nprint(greet(name))\nprint(math.sqrt(16))\n";
fs.writeFileSync(file,source);fs.writeFileSync(path.join(project,'second.py'),'value = 42\n');
const floodFile=path.join(project,'output.py');fs.writeFileSync(floodFile,"for i in range(100000):\n    print(f'{i}: console output performance check')\nprint('FLOOD_DONE')\n");
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
const app=spawn(path.join(bundle,'EduCode.exe'),[project],{windowsHide:true,env:{...process.env,EDUCODE_TEST_MODE:'1',QT_OPENGL:'software',QTWEBENGINE_REMOTE_DEBUGGING:'9237',EDUCODE_LOG:path.join(root,'out/project-create.log'),EDUCODE_SCREENSHOT:path.join(root,'out/verified-new-project.png'),EDUCODE_SCREENSHOT_DELAY:'22000'}});
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
 const editor=await connect('editor.html');await editor.wait('typeof editor!=="undefined"&&editor&&typeof host!=="undefined"&&host&&!!editor.getModel()');
 const base=path.join(root,'.runtime/create-project-tests');fs.mkdirSync(base,{recursive:true});
 const name='new-project-'+Date.now(),destination=path.join(base,name),py='C:/Users/zetsu/AppData/Local/Programs/Python/Python310/python.exe';
 await editor.eval('host.createProject('+JSON.stringify(name)+','+JSON.stringify(py)+','+JSON.stringify(base)+');true');
 for(let i=0;i<150;i++){if(await editor.eval('host.projectName==='+JSON.stringify(name)+'&&!!editor.getModel()&&editor.getModel().getValue().includes("name = input")'))break;if(i===149)throw new Error('New project did not open its starter script');await sleep(100);}
 assert(fs.existsSync(path.join(destination,'.venv/Scripts/python.exe')));
 assert.equal(await editor.eval('editor.getModel().getValue()'),fs.readFileSync(path.join(destination,'main.py'),'utf8'));
 assert((await editor.eval('editor.getModel().getValue()')).includes('Привет'));
 await editor.wait('host.runTargets.some(t=>t.name==="main.py")');console.log('PASS chosen project destination, immediate starter text and run selector');
 await editor.eval('host.command('+JSON.stringify('test-ui-shot '+path.join(root,'out/verified-new-project.png'))+');true');
 editor.command('Runtime.evaluate',{expression:'host.requestQuit();true'}).catch(()=>{});assert.equal(await exit,0);editor.close();
}
main().catch(e=>{console.error(e);if(!exited)app.kill();process.exitCode=1;});
