'use strict';
const consoleMode=location.search.includes('console');
const term=new Terminal({fontFamily:'Cascadia Code, Consolas, monospace',fontSize:13,lineHeight:1.3,cursorBlink:true,scrollback:5000,convertEol:consoleMode,theme:{background:'#1C1D1F',foreground:'#CDD0D5',cursor:'#CDD0D5',selectionBackground:'#3A4D66'}});
const fit=new FitAddon.FitAddon();term.loadAddon(fit);term.open(document.getElementById('terminal'));fit.fit();
let host,buffer='',backlog=0,queued='',scheduled=false;
function output(s){queued+=s;if(!scheduled){scheduled=true;requestAnimationFrame(()=>{scheduled=false;const data=queued;queued='';backlog+=data.length;term.write(data,()=>{backlog-=data.length;});});}}
new QWebChannel(qt.webChannelTransport,c=>{
 host=c.objects.backend;
 function applySettings(){term.options.fontSize=host.configuration["terminal.fontSize"];term.options.scrollback=host.configuration["terminal.scrollback"];fit.fit();if(!consoleMode)host.terminalResize(term.cols,term.rows);}
 host.configurationChanged.connect(applySettings);applySettings();
 if(consoleMode){host.consoleOutput.connect(output);term.onData(data=>{if(!host.running)return;for(const char of data.replace(/\r\n/g,'\r')){if(char==='\r'||char==='\n'){host.consoleInput(buffer+'\n');term.write('\r\n');buffer='';}else if(char==='\x7f'){if(buffer.length){buffer=buffer.slice(0,-1);term.write('\b \b');}}else if(char==='\x03'){host.stop();buffer='';}else if(char>=' '){buffer+=char;term.write(char);}}});host.stateChanged.connect(()=>{if(!host.running)buffer='';});}
 else {host.terminalOutput.connect(output);term.onData(s=>host.terminalInput(s));host.startTerminal();host.terminalResize(term.cols,term.rows);}
});
new ResizeObserver(()=>{fit.fit();if(host&&!consoleMode)host.terminalResize(term.cols,term.rows);}).observe(document.getElementById('terminal'));
document.getElementById('terminal').addEventListener('mousedown',()=>term.focus());

const menu=document.getElementById('context-menu');
function dismissMenu(){menu.style.display='none';}
const actions=[['Копировать',()=>{if(host)host.copyText(term.getSelection());}],['Вставить',()=>{if(host)host.clipboardText(text=>term.paste(text));}],['Выделить всё',()=>term.selectAll()],['Очистить',()=>term.clear()]];
for(const [title,run] of actions){const button=document.createElement('button');button.textContent=title;button.onclick=()=>{run();dismissMenu();term.focus();};menu.appendChild(button);}
document.addEventListener('contextmenu',event=>{event.preventDefault();menu.style.display='block';menu.style.left=Math.min(event.clientX,innerWidth-menu.offsetWidth-4)+'px';menu.style.top=Math.min(event.clientY,innerHeight-menu.offsetHeight-4)+'px';menu.firstChild.focus();});
document.addEventListener('mousedown',event=>{if(!menu.contains(event.target))dismissMenu();});
document.addEventListener('keydown',event=>{if(event.key==='Escape')dismissMenu();});
