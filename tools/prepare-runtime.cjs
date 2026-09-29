// Copy only distributable runtime files, keeping the desktop bundle small.
const fs=require('fs'),path=require('path');
const source=process.argv[2],target=process.argv[3];
for (const entry of ['monaco-editor/min/vs/loader.js','pyright/dist/pyright-langserver.js','xterm/lib/xterm.js','node-pty/lib/index.js']) {
 if (!fs.existsSync(path.join(source,'node_modules',entry))) throw new Error('Missing runtime dependency '+entry+'. Run npm ci in the source directory.');
}
function copy(from,to){
 const stat=fs.statSync(from);
 if(stat.isDirectory()){fs.mkdirSync(to,{recursive:true});for(const name of fs.readdirSync(from))copy(path.join(from,name),path.join(to,name));}
 else {if(fs.existsSync(to)){const old=fs.statSync(to);if(old.size===stat.size&&Math.abs(old.mtimeMs-stat.mtimeMs)<2)return;}fs.mkdirSync(path.dirname(to),{recursive:true});fs.copyFileSync(from,to);fs.utimesSync(to,stat.atime,stat.mtime);}
}
for(const part of ['monaco-editor/min','monaco-editor/LICENSE','monaco-editor/ThirdPartyNotices.txt','pyright/dist','pyright/LICENSE.txt','xterm/lib','xterm/css','xterm/LICENSE','xterm-addon-fit/lib','xterm-addon-fit/LICENSE','@vscode/codicons/dist','@vscode/codicons/LICENSE','node-pty/lib','node-pty/prebuilds','node-pty/build/Release','node-pty/package.json','node-pty/LICENSE']){
 const from=path.join(source,'node_modules',part);if(fs.existsSync(from))copy(from,path.join(target,'node_modules',part));
}
for(const dir of ['qml','web','assets','tools','docs','plugins'])copy(path.join(source,dir),path.join(target,dir));
if(process.platform==='win32')copy(process.execPath,path.join(target,'node.exe'));
