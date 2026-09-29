const {spawn,spawnSync}=require('child_process'),fs=require('fs'),path=require('path'),assert=require('assert');
const root=path.resolve(__dirname,'..'),linux=process.platform==='linux';
const project=path.join(root,'.runtime/import-regression');fs.mkdirSync(project,{recursive:true});
const main=path.join(project,'main.py'),moduleFile=path.join(project,'test.py');
fs.writeFileSync(main,'import test\ntest.hello()\n');fs.writeFileSync(moduleFile,'# empty module\n');
if(!fs.existsSync(path.join(project,'.venv')))assert.equal(spawnSync(linux?'python3':'python',['-m','venv',path.join(project,'.venv')]).status,0);
const env={...process.env,EDUCODE_TEST_MODE:'1',EDUCODE_TEST_AI_HELPER:path.join(root,'tests/mock-ai-agent.py'),QTWEBENGINE_REMOTE_DEBUGGING:'9243'};
if(linux){for(const line of spawnSync('systemctl',['--user','show-environment'],{encoding:'utf8'}).stdout.split('\n')){const i=line.indexOf('=');if(i>0)env[line.slice(0,i)]=line.slice(i+1);}env.XDG_RUNTIME_DIR='/run/user/'+process.getuid();}
const binary=path.join(root,'out/build',linux?'linux-release':'educode-qt5-release','EduCode',linux?'EduCode':'EduCode.exe');
const app=spawn(binary,[main,'--line','2','--column','3'],{env,windowsHide:true,stdio:'ignore'});
const sleep=ms=>new Promise(r=>setTimeout(r,ms));let ws;const pending=new Map();let seq=0;
async function evaluate(expression){return new Promise((resolve,reject)=>{const id=++seq;pending.set(id,{resolve,reject});ws.send(JSON.stringify({id,method:'Runtime.evaluate',params:{expression,returnByValue:true,awaitPromise:true}}));setTimeout(()=>{if(pending.delete(id))reject(Error('CDP timeout'));},12000).unref();});}
async function wait(expression){for(let i=0;i<120;i++){if(await evaluate(expression))return;await sleep(200);}throw Error('Timeout: '+expression);}
(async()=>{let page;for(let i=0;i<100;i++){try{page=(await(await fetch('http://127.0.0.1:9243/json/list')).json()).find(p=>p.url.includes('editor.html'));if(page)break;}catch{}await sleep(200);}assert(page,'editor unavailable');
 const Socket=global.WebSocket||require('ws');ws=new Socket(page.webSocketDebuggerUrl);await new Promise((resolve,reject)=>{ws.onopen=resolve;ws.onerror=reject;});
 ws.onmessage=e=>{const m=JSON.parse(e.data);const p=pending.get(m.id);if(!p)return;pending.delete(m.id);m.error||m.result.exceptionDetails?p.reject(Error(JSON.stringify(m))):p.resolve(m.result.result.value);};
 await wait('typeof host!=="undefined"&&host&&serverReady&&activePath.endsWith("main.py")');
 assert.deepEqual(await evaluate('editor.getPosition()'),{lineNumber:2,column:3});console.log('PASS CLI line/column');
 await wait('(diagnostics.get(uri(activePath))||[]).some(d=>d.message.includes("hello"))');
 await evaluate('host.openFile('+JSON.stringify(moduleFile)+');true');await wait('activePath.endsWith("test.py")');
 await evaluate('editor.getModel().setValue('+JSON.stringify('def hello():\n    print("IMPORT_OK")\n')+');true');await sleep(600);
 await evaluate('host.openFile('+JSON.stringify(main)+');true');await wait('activePath.endsWith("main.py")');
 await wait('(diagnostics.get(uri(activePath))||[]).length===0');
 const definition=await evaluate('request("textDocument/definition",{textDocument:{uri:uri(activePath)},position:{line:1,character:7}})');assert(JSON.stringify(definition).includes('test.py'));
 console.log('PASS unsaved local module survives tab switch; definition resolves');
 await evaluate('editor.getModel().setValue("import test\\ntest.missing_function()\\n");true');
 await wait('(diagnostics.get(uri(activePath))||[]).some(d=>d.message.includes("missing_function"))');console.log('PASS genuine unknown attribute still reported');
 await evaluate('editor.getModel().setValue("import test\\ntest.hello()\\n");host.saveAll();true');
 assert(fs.readFileSync(moduleFile,'utf8').includes('def hello'));console.log('PASS save imported module');
 const configPath=await evaluate('host.configPath');
 const marker=path.join(path.dirname(configPath),'ai-quota/offline-retry.marker');if(fs.existsSync(marker))fs.unlinkSync(marker);
 await evaluate('host.setSetting("ai.provider","groq");host.setSetting("ai.groqApiKey","offline-test-key");host.setSetting("ai.groqModel","offline-test-model");host.ai.createChat();host.ai.send("offline retry test");true');
 await sleep(500);await wait('!host.ai.busy&&host.ai.messages.length===1');
 await evaluate('host.ai.retry();true');await wait('!host.ai.busy&&host.ai.messages.length===2');
 assert.equal(await evaluate('host.ai.messages[1].content'),'OFFLINE_RETRY_OK');
 await evaluate('host.setSetting("ai.groqApiKey","");host.setSetting("ai.provider","ollama");true');console.log('PASS failed AI request retry without duplicated prompt (zero API calls)');
})().then(()=>{ws.close();app.kill();}).catch(e=>{console.error(e);if(ws)ws.close();app.kill();process.exitCode=1;});
