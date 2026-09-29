const {spawn}=require('child_process'),fs=require('fs'),path=require('path'),assert=require('assert');
const root=path.resolve(__dirname,'..'),bundle=process.env.EDUCODE_TEST_BUNDLE||path.join(root,'out/build/educode-qt5-release/EduCode'),project=path.join(root,'.runtime/smoke-project');
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
const app=spawn(path.join(bundle,'EduCode.exe'),[project],{windowsHide:true,env:{...process.env,EDUCODE_TEST_MODE:'1',QT_OPENGL:'software',QTWEBENGINE_REMOTE_DEBUGGING:'9237',EDUCODE_LOG:path.join(root,'out/agent-test.log'),EDUCODE_SCREENSHOT:path.join(root,'out/agent-final.png'),EDUCODE_SCREENSHOT_DELAY:'120000'}});
let exited=false;const exit=new Promise(r=>app.on('exit',c=>{exited=true;r(c);}));
class Client{constructor(url){this.ws=new WebSocket(url);this.seq=0;this.pending=new Map();this.connected=new Promise((r,j)=>{this.ws.onopen=r;this.ws.onerror=j});this.ws.onmessage=e=>{let m=JSON.parse(e.data),p=this.pending.get(m.id);if(p){this.pending.delete(m.id);m.error?p.j(new Error(JSON.stringify(m.error))):p.r(m.result)}}}async command(method,params={}){await this.connected;return new Promise((r,j)=>{let id=++this.seq,t=setTimeout(()=>j(new Error('timeout '+method)),15000);this.pending.set(id,{r:v=>{clearTimeout(t);r(v)},j});this.ws.send(JSON.stringify({id,method,params}))})}async eval(expression){let x=await this.command('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});if(x.exceptionDetails)throw Error(JSON.stringify(x.exceptionDetails));return x.result.value}async wait(expr,tries=120){for(let i=0;i<tries;i++){if(await this.eval(expr))return;await sleep(250)}throw Error('wait '+expr)}close(){this.ws.close()}}
async function connect(){for(let i=0;i<60;i++){try{let pages=await(await fetch('http://127.0.0.1:9237/json/list')).json(),p=pages.find(x=>x.url.includes('editor.html'));if(p)return new Client(p.webSocketDebuggerUrl)}catch{}await sleep(200)}throw Error('editor missing')}
async function main(){
 const editor=await connect();await editor.wait('typeof host!=="undefined"&&host&&host.projectPath&&typeof editor!=="undefined"&&editor&&!!editor.getModel()');
 await editor.eval('host.packages.refresh();true');await editor.wait('!host.packages.busy&&host.packages.items.length>0');
 assert(await editor.eval('host.packages.items.some(p=>p.name.toLowerCase()==="pip")'));console.log('PASS installed packages');
 await editor.eval('host.packages.search("requests");true');await editor.wait('!host.packages.busy&&host.packages.items.some(p=>p.name.toLowerCase()==="requests")',240);
 await editor.eval('host.packages.select("requests");true');await editor.wait('!host.packages.busy&&host.packages.details.name&&host.packages.details.name.toLowerCase()==="requests"',240);
 assert((await editor.eval('host.packages.details.summary')).length>0);console.log('PASS PyPI search and metadata');
 await editor.eval('host.packages.search("");host.packages.install("colorama");true');await editor.wait('!host.packages.busy&&host.packages.items.some(p=>p.name.toLowerCase()==="colorama"&&p.installed)',240);console.log('PASS package install');
 await editor.eval('host.packages.remove("colorama");true');await editor.wait('!host.packages.busy&&!host.packages.items.some(p=>p.name.toLowerCase()==="colorama"&&p.installed)',240);console.log('PASS package remove');
 await editor.eval('host.setSetting("ai.model","qwen2.5:7b");true');await editor.wait('host.ai.ollamaAvailable&&host.ai.models.some(m=>m.name==="qwen2.5:7b")');
 await editor.eval('host.ai.send("Ответь одним коротким предложением: как тебя зовут и где ты работаешь? Не используй инструменты.");true');
 await editor.wait('!host.ai.busy&&host.ai.messages.some(m=>m.role==="assistant")',240);
 const answer=await editor.eval('host.ai.messages.filter(m=>m.role==="assistant").slice(-1)[0].content');assert(/Лир/i.test(answer)&&/EduCode/i.test(answer));console.log('PASS Ollama chat, identity and persistence');
 await editor.eval('host.ai.send("Используй инструмент notify и отправь уведомление с заголовком Agent test и текстом Готово. Затем кратко подтверди.");true');
 await editor.wait('!host.ai.busy&&host.notifications.some(n=>n.title==="Agent test")',240);console.log('PASS AI tool call through restricted IDE API');
 await editor.eval('host.command("test-ui-click 1258 172 left");true');await sleep(500);await editor.eval('host.command('+JSON.stringify('test-ui-shot '+path.join(root,'out/verified-ai-agent.png'))+');true');
 await editor.eval('host.command("test-ui-click 690 580 left");true');await sleep(500);await editor.eval('host.command('+JSON.stringify('test-ui-shot '+path.join(root,'out/verified-packages.png'))+');true');
 editor.close();app.kill();await exit;
}
main().then(()=>console.log('PASS package manager and Lira agent')).catch(e=>{console.error(e);if(!exited)app.kill();process.exitCode=1});
