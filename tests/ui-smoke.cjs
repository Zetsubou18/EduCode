// Actual Qt WebEngine integration tests; CDP listens on localhost only during this test.
const {spawn,spawnSync}=require('child_process');
const fs=require('fs'),path=require('path'),assert=require('assert');
const root=path.resolve(__dirname,'..'),bundle=process.env.EDUCODE_TEST_BUNDLE||path.join(root,'out/build/educode-qt5-release/EduCode');
const project=path.join(root,'.runtime/smoke-project'),file=path.join(project,'main.py');
const source="import math\n\ndef greet(name: str) -> str:\n    return f'Привет, {name}!'\n\nname = input('Имя: ')\nprint(greet(name))\nprint(math.sqrt(16))\n";
fs.writeFileSync(file,source);fs.writeFileSync(path.join(project,'second.py'),'value = 42\n');
const floodFile=path.join(project,'output.py');fs.writeFileSync(floodFile,"for i in range(100000):\n    print(f'{i}: console output performance check')\nprint('FLOOD_DONE')\n");
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
const app=spawn(path.join(bundle,'EduCode.exe'),[project],{windowsHide:true,env:{...process.env,EDUCODE_TEST_MODE:'1',QT_OPENGL:'software',QTWEBENGINE_REMOTE_DEBUGGING:'9237',EDUCODE_LOG:path.join(root,'out/ui-smoke-final.log'),EDUCODE_SCREENSHOT:path.join(root,'out/verified-ide.png'),EDUCODE_SCREENSHOT_DELAY:'30000'}});
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
 await editor.wait('typeof editor!=="undefined"&&editor&&typeof host!=="undefined"&&host&&serverReady&&!!editor.getModel()&&innerWidth>100');
 assert((await editor.eval('editor.getModel().getValue()')).includes('greet'));
 const completion=await editor.eval('request("textDocument/completion",{textDocument:{uri:uri(activePath)},position:{line:7,character:10}})');
 assert((completion.items||completion).some(i=>i.label==='math'));console.log('PASS autocomplete');
 const hover=await editor.eval('request("textDocument/hover",{textDocument:{uri:uri(activePath)},position:{line:6,character:8}})');assert(hover.contents.value.includes('greet'));console.log('PASS hover/signature');
 const signature=await editor.eval('request("textDocument/signatureHelp",{textDocument:{uri:uri(activePath)},position:{line:6,character:12}})');assert(signature.signatures[0].label.includes('name'));console.log('PASS parameter hints');
 const tabCount=(await editor.eval('host.tabs')).length;
 await editor.eval('editor.setPosition({lineNumber:7,column:9});editor.getAction("editor.action.revealDefinition").run()');
 await editor.wait('editor.getPosition().lineNumber===3');assert.equal((await editor.eval('host.tabs')).length,tabCount);await editor.eval('host.navigateBack();true');await editor.wait('editor.getPosition().lineNumber===7');console.log('PASS definition and navigation history');
 const originalPath=await editor.eval('activePath');const modified=source+'\nundefined_symbol()\n';
 await editor.eval('editor.getModel().setValue('+JSON.stringify(modified)+');flushChange();true');
 await editor.wait('host.problems.some(p=>p.message.includes("undefined_symbol"))');assert((await editor.eval('monaco.editor.getModelMarkers({owner:"pyright"}).length'))>0);console.log('PASS diagnostics and Problems');
 await editor.eval('host.openFile('+JSON.stringify(path.join(project,'second.py'))+');true');await editor.wait('activePath.endsWith("second.py")');
 await editor.eval('host.openFile('+JSON.stringify(originalPath)+');true');await editor.wait('activePath.endsWith("main.py")');assert.equal(await editor.eval('editor.getModel().getValue()'),modified);console.log('PASS modified text across tabs');
 await editor.eval('host.home();true');await sleep(200);assert.equal(await editor.eval('host.page'),'ide');await editor.eval('host.resolveUnsaved("cancel");true');console.log('PASS unsaved Home protection');
 await editor.eval('editor.getModel().setValue('+JSON.stringify(source)+');host.saveAll();true');await sleep(250);assert.equal(fs.readFileSync(file,'utf8'),source);console.log('PASS atomic save');
 await editor.eval('host.settings();true');await sleep(150);assert.equal(await editor.eval('host.page'),'settings');await editor.eval('host.back();true');await sleep(150);assert.equal(await editor.eval('host.page'),'ide');console.log('PASS Settings/back');
 const consolePage=await connect('terminal.html?console');await consolePage.wait('typeof host!=="undefined"&&host&&term.cols>10');
 await editor.eval('host.run();true');await consolePage.wait('host.running');await consolePage.wait(textBuffer+'.includes("Имя:")');
 await consolePage.eval('term.focus();true');await consolePage.command('Input.insertText',{text:'Тест'});await consolePage.eval('term.textarea.dispatchEvent(new KeyboardEvent("keydown",{key:"Enter",code:"Enter",keyCode:13,which:13,bubbles:true}));true');
 await consolePage.wait('!host.running');assert((await consolePage.eval(textBuffer)).includes('Привет, Тест!'));console.log('PASS real console keyboard input and Unicode');
 await editor.eval('host.command("terminal");true');const terminal=await connect('terminal.html');await terminal.wait('typeof host!=="undefined"&&host&&term.cols>10');await terminal.wait(textBuffer+'.includes(">")');
 await terminal.eval('host.terminalInput('+JSON.stringify('python -c "import sys; print(\'PTY_OK\', sys.prefix)"\r')+');true');try{await terminal.wait(textBuffer+'.includes("PTY_OK")&&'+textBuffer+'.includes(".venv")');}catch(e){console.log('Terminal buffer:',await terminal.eval(textBuffer));throw e;}console.log('PASS PTY shell and project Python');
 const image=await terminal.command('Page.captureScreenshot',{format:'png'});fs.writeFileSync(path.join(root,'out/verified-terminal.png'),Buffer.from(image.data,'base64'));
 await editor.eval('host.command("console");true');
 let clock=Date.now();const native=spawnSync(path.join(project,'.venv/Scripts/python.exe'),['-u',floodFile],{windowsHide:true,maxBuffer:12*1024*1024});assert.equal(native.status,0);const nativeTime=Date.now()-clock;
 await editor.eval('host.openFile('+JSON.stringify(floodFile)+');true');await editor.wait('activePath.endsWith("output.py")');clock=Date.now();await editor.eval('host.run();true');await consolePage.wait(textBuffer+'.includes("FLOOD_DONE")');const uiTime=Date.now()-clock;
 console.log('PASS 100000 lines through live console: native '+nativeTime+' ms; IDE display '+uiTime+' ms');assert(uiTime<nativeTime*3+1000);
 await editor.eval('host.openFile('+JSON.stringify(path.join(project,'second.py'))+');true');await editor.wait('activePath.endsWith("second.py")');await editor.eval('editor.getModel().setValue("import time\\nprint(\'RUNNING\',flush=True)\\ntime.sleep(60)\\n");host.run();true');await consolePage.wait('host.running');await consolePage.eval('host.stop();true');await consolePage.wait('!host.running');console.log('PASS stop running process');
 await editor.eval('editor.getModel().setValue("value = 42\\n");host.saveAll();host.openFile('+JSON.stringify(file)+');true');await editor.wait('activePath.endsWith("main.py")');await editor.eval('host.run();true');await consolePage.wait('host.running');await consolePage.eval('host.consoleInput("Тест\\n");true');await consolePage.wait('!host.running');
 // Force a genuine suggestion popup near the bottom edge and assert it misses the caret line.
 const popupText=source+'\n'.repeat(14)+'math.';
 await editor.eval('editor.getModel().setValue('+JSON.stringify(popupText)+');editor.setPosition({lineNumber:editor.getModel().getLineCount(),column:6});editor.revealPositionInCenter(editor.getPosition());editor.getAction("editor.action.triggerSuggest").run();true');
 await editor.wait('!!document.querySelector(".suggest-widget.visible")');
 const geometry=await editor.eval('(()=>{const r=document.querySelector(".suggest-widget.visible").getBoundingClientRect();const p=editor.getScrolledVisiblePosition(editor.getPosition());return {top:r.top,bottom:r.bottom,left:r.left,right:r.right,caret:p.top,lineHeight:p.height,w:innerWidth,h:innerHeight};})()');
 console.log('Popup geometry:',geometry);
 const shot=await editor.command('Page.captureScreenshot',{format:'png'});fs.writeFileSync(path.join(root,'out/verified-editor-popup.png'),Buffer.from(shot.data,'base64'));
 assert(geometry.top>=geometry.caret+geometry.lineHeight-1||geometry.bottom<=geometry.caret+1);assert(geometry.left>=0&&geometry.right<=geometry.w+1);console.log('PASS suggestion popup geometry');
 await editor.eval('editor.trigger("test","hideSuggestWidget",{});editor.getModel().setValue('+JSON.stringify(source)+');host.saveAll();editor.setPosition({lineNumber:7,column:9});true');
 await editor.eval('host.closeTab('+JSON.stringify(path.join(project,'second.py').replace(/\\/g,'/'))+');true');await sleep(300);
 console.log('All integration assertions passed; capturing full window.');
 const result=await Promise.race([exit,sleep(35000).then(()=>{app.kill();throw new Error('Screenshot timeout');})]);assert.equal(result,0);
 editor.close();consolePage.close();terminal.close();
}
main().then(()=>console.log('PASS full UI smoke')).catch(e=>{console.error(e);if(!exited)app.kill();process.exitCode=1;});
