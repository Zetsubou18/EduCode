'use strict';
require.config({'vs/nls':{availableLanguages:{'*':new URLSearchParams(location.search).get('lang')==='en'?'en':'ru'}},paths:{vs:'../node_modules/monaco-editor/min/vs'}});
let host, editor, secondEditor, activePath='', splitPath='', serverReady=false, requestId=0, changing=false, changeTimer;
const openedLsp=new Set();
const models=new Map(), versions=new Map(), requests=new Map(), diagnostics=new Map();
const pluginLanguages=new Map(), registeredLanguages=new Set();
const uri=p=>monaco.Uri.file(p).toString();
const pathOf=u=>monaco.Uri.parse(u).fsPath.replace(/\\/g,'/');
const position=p=>({line:p.lineNumber-1,character:p.column-1});
const range=r=>({startLineNumber:r.start.line+1,startColumn:r.start.character+1,endLineNumber:r.end.line+1,endColumn:r.end.character+1});
const isPython=p=>/\.pyi?$/i.test(p);
const language=p=>isPython(p)?'python':/\.json$/i.test(p)?'json':/\.md$/i.test(p)?'markdown':/requirements\.txt$/i.test(p)?'requirements':/(^|[/\\])\.env(?:\..*)?$/i.test(p)?'env':/(^|[/\\])\.gitignore$/i.test(p)?'gitignore':pluginLanguages.get((p.match(/\.[^.\/\\]+$/)||[''])[0].toLowerCase())||'plaintext';
function refreshPluginLanguages(){pluginLanguages.clear();for(const plugin of host.plugins){if(!plugin.enabled)continue;for(const item of plugin.languages||[]){const ext=item.extension.startsWith('.')?item.extension:'.'+item.extension;pluginLanguages.set(ext.toLowerCase(),item.id);if(!registeredLanguages.has(item.id)){registeredLanguages.add(item.id);monaco.languages.register({id:item.id});const words=new Set(item.keywords||[]);monaco.languages.setMonarchTokensProvider(item.id,{tokenizer:{root:[[/[a-zA-Z_][\w]*/,{cases:{'@keywords':'keyword','@default':'identifier'}}],[/\d+/, 'number'],[/#.*/, 'comment']]},keywords:[...words]});}}}for(const [path,model] of models)monaco.editor.setModelLanguage(model,language(path));}
const escapeHtml=s=>s.replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
function renderMarkdown(source){let code=false;return source.split('\n').map(line=>{if(/^```/.test(line)){code=!code;return code?'<pre><code>':'</code></pre>';}if(code)return escapeHtml(line)+'\n';let s=escapeHtml(line);s=s.replace(/\[([^\]]+)\]\((https?:\/\/[^\s)]+)\)/g,(_,label,url)=>'<a href="'+url+'">'+label+'</a>').replace(/\*\*([^*]+)\*\*/g,'<strong>$1</strong>').replace(/`([^`]+)`/g,'<code>$1</code>');const heading=s.match(/^(#{1,6})\s+(.+)$/);if(heading)return '<h'+heading[1].length+'>'+heading[2]+'</h'+heading[1].length+'>';if(/^\s*[-*] /.test(s))return '<li>'+s.replace(/^\s*[-*] /,'')+'</li>';if(/^> /.test(s))return '<blockquote>'+s.slice(2)+'</blockquote>';return s?'<p>'+s+'</p>':'';}).join('');}
function updateToolbar(){const md=/\.md$/i.test(activePath),req=/requirements\.txt$/i.test(activePath);document.getElementById('toolbar').style.display=md||req?'flex':'none';document.getElementById('workspace').style.height=md||req?'calc(100% - 34px)':'100%';document.getElementById('codeButton').style.display=md?'':'none';document.getElementById('previewButton').style.display=md?'':'none';document.getElementById('installButton').style.display=req?'':'none';if(!md)showPreview(false);}
function updateToolbarLabels(){if(!host)return;const en=host.configuration['general.language']==='en';document.getElementById('codeButton').textContent=en?'Code':'Код';document.getElementById('previewButton').textContent=en?'Preview':'Просмотр';document.getElementById('installButton').textContent=en?'Install all':'Установить все';}
function showPreview(on){document.getElementById('preview').style.display=on?'block':'none';document.getElementById('editor').style.display=on?'none':'block';if(on&&models.has(activePath))document.getElementById('preview').innerHTML=renderMarkdown(models.get(activePath).getValue());else editor.layout();}
function toggleSplit(){if(secondEditor){secondEditor.dispose();secondEditor=null;splitPath='';document.getElementById('secondary').style.display='none';document.getElementById('editor').style.width='100%';}else if(activePath){splitPath=activePath;secondEditor=monaco.editor.create(document.getElementById('secondary'),{model:models.get(splitPath),theme:'educode',automaticLayout:true,minimap:{enabled:false},fontSize:host.configuration['editor.fontSize'],scrollBeyondLastLine:false});document.getElementById('secondary').style.display='block';document.getElementById('editor').style.width='50%';document.getElementById('secondary').style.width='50%';}editor.layout();}
function send(method,params,id){if(!serverReady)return;const msg={jsonrpc:'2.0',method,params};if(id!==undefined)msg.id=id;host.lspSend(JSON.stringify(msg));}
function request(method,params,token){
  if(!serverReady)return Promise.resolve(null);flushChange();
  return new Promise(resolve=>{const id=++requestId;const timer=setTimeout(()=>{requests.delete(id);resolve(null);},8000);requests.set(id,{resolve,timer});send(method,params,id);if(token)token.onCancellationRequested(()=>{send('$/cancelRequest',{id});clearTimeout(timer);requests.delete(id);resolve(null);});});
}
function flushChange(path=activePath){clearTimeout(changeTimer);if(!path||!isPython(path)||!models.has(path)||!serverReady)return;const m=models.get(path);const v=m.getVersionId();if(versions.get(path)===v)return;versions.set(path,v);send('textDocument/didChange',{textDocument:{uri:uri(path),version:v},contentChanges:[{text:m.getValue()}]});}
function openLsp(path=activePath){
  if(!path||!isPython(path)||!serverReady||!models.has(path)||openedLsp.has(path))return;
  const m=models.get(path);versions.set(path,m.getVersionId());openedLsp.add(path);
  send('textDocument/didOpen',{textDocument:{uri:uri(path),languageId:m.getLanguageId(),version:m.getVersionId(),text:m.getValue()}});
}
function reportProblems(){const ds=diagnostics.get(uri(activePath))||[];host.setProblems(activePath,JSON.stringify(ds.map(d=>({severity:d.severity||1,message:d.message,line:d.range.start.line+1,column:d.range.start.character+1,path:activePath}))));}
function activate(path,text,line,column){
  if(activePath!==path)flushChange();
  let model=models.get(path), textChanged=false;
  if(!model){model=monaco.editor.createModel(text,language(path),monaco.Uri.file(path));models.set(path,model);model.onDidChangeContent(()=>{if(changing)return;host.editDocument(path,model.getValue());if(path===activePath||path===splitPath){clearTimeout(changeTimer);changeTimer=setTimeout(()=>flushChange(path),180);}if(document.getElementById('preview').style.display==='block'&&path===activePath)showPreview(true);});}
  else if(model.getValue()!==text){
    changing=true;
    try{model.pushEditOperations([],[{range:model.getFullModelRange(),text}],()=>null);textChanged=true;}finally{changing=false;}
  }
  const old=editor.getModel();if(old&&activePath)old._eduView=editor.saveViewState();const switched=activePath!==path;activePath=path;editor.setModel(model);if(model._eduView)editor.restoreViewState(model._eduView);
  if(switched)openLsp();else if(textChanged)flushChange();if(line>0){column=Math.max(1,column);editor.setPosition({lineNumber:line,column});editor.revealPositionInCenter({lineNumber:line,column});}
  if(host&&models.has(path))models.get(path).updateOptions({tabSize:host.configuration["editor.tabSize"]});
  if(!isPython(path))diagnostics.delete(uri(path));reportProblems();updateToolbar();editor.focus();
}
require(['vs/editor/editor.main'],()=>{
 monaco.languages.register({id:'requirements'});monaco.languages.setMonarchTokensProvider('requirements',{tokenizer:{root:[[/^\s*#.*/, 'comment'],[/^[A-Za-z0-9_.-]+/, 'type.identifier'],[/(==|>=|<=|~=|!=|>|<)/,'keyword'],[/\d+(?:\.\d+)*/, 'number']]}});
 monaco.languages.register({id:'env'});monaco.languages.setMonarchTokensProvider('env',{tokenizer:{root:[[/^\s*#.*/, 'comment'],[/^[A-Za-z_][A-Za-z0-9_]*(?==)/,'type.identifier'],[/=.*/, 'string']]}});
 monaco.languages.register({id:'gitignore'});monaco.languages.setMonarchTokensProvider('gitignore',{tokenizer:{root:[[/^\s*#.*/, 'comment'],[/^!.*$/, 'keyword'],[/[^\s]+/, 'string']]}});
 monaco.editor.defineTheme('educode',{base:'vs-dark',inherit:true,rules:[{token:'keyword',foreground:'CE9178'},{token:'string',foreground:'9DB98F'},{token:'number',foreground:'B5CEA8'},{token:'comment',foreground:'747A80'},{token:'type.identifier',foreground:'86B8C9'}],colors:{'editor.background':'#202123','editor.foreground':'#D5D8DC','editorLineNumber.foreground':'#646970','editorLineNumber.activeForeground':'#CDD2D9','editor.lineHighlightBackground':'#292B2E','editor.selectionBackground':'#3A4D66','editorCursor.foreground':'#CDD7E7','editorWidget.background':'#2A2C30','editorWidget.border':'#45484E','editorSuggestWidget.selectedBackground':'#3A424E','editorHoverWidget.background':'#2A2C30','editorHoverWidget.border':'#45484E','menu.background':'#2A2C30','menu.foreground':'#D5D8DC','menu.border':'#45484E','menu.selectionBackground':'#3A424E','menu.selectionForeground':'#D5D8DC','focusBorder':'#566D8E'}});
 editor=monaco.editor.create(document.getElementById('editor'),{theme:'educode',fontFamily:'Cascadia Code, Consolas, monospace',fontSize:14,lineHeight:23,minimap:{enabled:false},padding:{top:18,bottom:16},scrollBeyondLastLine:false,automaticLayout:true,tabSize:4,insertSpaces:true,renderLineHighlight:'line',overviewRulerBorder:false,quickSuggestions:{other:true,comments:false,strings:false},quickSuggestionsDelay:180,suggestOnTriggerCharacters:true,wordBasedSuggestions:false,suggest:{showWords:false,preview:false,selectionMode:'always'},hover:{delay:650,above:true},parameterHints:{enabled:true},fixedOverflowWidgets:false,smoothScrolling:true,mouseWheelZoom:true});
 monaco.languages.registerCompletionItemProvider('python',{triggerCharacters:['.'],provideCompletionItems:async(model,p,ctx,token)=>{
   const result=await request('textDocument/completion',{textDocument:{uri:model.uri.toString()},position:position(p),context:{triggerKind:ctx.triggerKind,triggerCharacter:ctx.triggerCharacter}},token);
   if(token.isCancellationRequested)return {suggestions:[]};const word=model.getWordUntilPosition(p);const items=Array.isArray(result)?result:(result&&result.items)||[];
   const kinds={1:monaco.languages.CompletionItemKind.Text,2:monaco.languages.CompletionItemKind.Method,3:monaco.languages.CompletionItemKind.Function,4:monaco.languages.CompletionItemKind.Constructor,5:monaco.languages.CompletionItemKind.Field,6:monaco.languages.CompletionItemKind.Variable,7:monaco.languages.CompletionItemKind.Class,9:monaco.languages.CompletionItemKind.Module,10:monaco.languages.CompletionItemKind.Property,14:monaco.languages.CompletionItemKind.Keyword,21:monaco.languages.CompletionItemKind.Constant};
   return {suggestions:items.map(i=>({label:i.label,kind:kinds[i.kind]||monaco.languages.CompletionItemKind.Variable,detail:i.detail,documentation:typeof i.documentation==='string'?i.documentation:i.documentation&&{value:i.documentation.value},insertText:i.textEdit?i.textEdit.newText:i.insertText||i.label,insertTextRules:i.insertTextFormat===2?monaco.languages.CompletionItemInsertTextRule.InsertAsSnippet:undefined,range:i.textEdit&&i.textEdit.range?range(i.textEdit.range):{startLineNumber:p.lineNumber,endLineNumber:p.lineNumber,startColumn:word.startColumn,endColumn:word.endColumn},additionalTextEdits:(i.additionalTextEdits||[]).map(e=>({range:range(e.range),text:e.newText})),sortText:i.sortText,filterText:i.filterText,_lsp:i})),incomplete:!!(result&&result.isIncomplete)};
 },resolveCompletionItem:async(item,token)=>{const i=await request('completionItem/resolve',item._lsp,token);if(i){item.detail=i.detail;item.documentation=typeof i.documentation==='string'?i.documentation:i.documentation&&{value:i.documentation.value};item.additionalTextEdits=(i.additionalTextEdits||[]).map(e=>({range:range(e.range),text:e.newText}));}return item;}});
 monaco.languages.registerHoverProvider('python',{provideHover:async(model,p,token)=>{const r=await request('textDocument/hover',{textDocument:{uri:model.uri.toString()},position:position(p)},token);if(!r)return null;const contents=(Array.isArray(r.contents)?r.contents:[r.contents]).filter(Boolean).map(c=>({value:typeof c==='string'?c:c.language?'```'+c.language+'\n'+c.value+'\n```':c.value}));return {contents,range:r.range&&range(r.range)};}});
 monaco.languages.registerSignatureHelpProvider('python',{signatureHelpTriggerCharacters:['(',','],signatureHelpRetriggerCharacters:[',',')'],provideSignatureHelp:async(model,p,token)=>{const r=await request('textDocument/signatureHelp',{textDocument:{uri:model.uri.toString()},position:position(p)},token);if(!r)return null;return {value:{activeSignature:r.activeSignature||0,activeParameter:r.activeParameter||0,signatures:r.signatures.map(s=>({...s,documentation:typeof s.documentation==='string'?s.documentation:s.documentation&&{value:s.documentation.value},parameters:s.parameters||[]}))},dispose(){}};}});
 monaco.languages.registerDefinitionProvider('python',{provideDefinition:async(model,p,token)=>{const r=await request('textDocument/definition',{textDocument:{uri:model.uri.toString()},position:position(p)},token);return (Array.isArray(r)?r:r?[r]:[]).map(d=>({uri:monaco.Uri.parse(d.uri||d.targetUri),range:range(d.range||d.targetSelectionRange)}));}});
 editor._codeEditorService.openCodeEditor=async(input,source)=>{const p=source.getPosition();const selection=input.options&&input.options.selection;const dest=selection?{line:selection.startLineNumber,column:selection.startColumn}:{line:1,column:1};host.navigate(pathOf(input.resource.toString()),dest.line,dest.column,activePath,p.lineNumber,p.column);return editor;};
 new QWebChannel(qt.webChannelTransport,channel=>{
  host=channel.objects.backend;
  host.sourceActivated.connect(activate);
  host.documentClosed.connect(path=>{if(activePath===path){flushChange();activePath='';editor.setModel(null);}if(splitPath===path)toggleSplit();if(openedLsp.delete(path))send('textDocument/didClose',{textDocument:{uri:uri(path)}});const m=models.get(path);if(m)m.dispose();models.delete(path);versions.delete(path);diagnostics.delete(uri(path));updateToolbar();});
  host.languageReady.connect(()=>{serverReady=true;openedLsp.clear();versions.clear();for(const path of models.keys())openLsp(path);});
  host.lspMessage.connect(json=>{const obj=JSON.parse(json);if(obj.id!==undefined){const req=requests.get(obj.id);if(req){clearTimeout(req.timer);requests.delete(obj.id);req.resolve(obj.result||null);}}else if(obj.method==='textDocument/publishDiagnostics'){const {uri:u,diagnostics:ds,version}=obj.params;const model=Array.from(models.values()).find(m=>m.uri.toString()===u);if(!model||!isPython(model.uri.fsPath))return;if(version!==undefined&&version<model.getVersionId())return;diagnostics.set(u,ds);monaco.editor.setModelMarkers(model,'pyright',ds.map(d=>({...range(d.range),message:d.message,severity:d.severity===1?monaco.MarkerSeverity.Error:d.severity===2?monaco.MarkerSeverity.Warning:monaco.MarkerSeverity.Info,source:d.source||'Pyright'})));if(u===uri(activePath))reportProblems();}});
  host.editorCommand.connect(name=>{if(name==='split'){toggleSplit();return;}const actions={find:'actions.find',replace:'editor.action.startFindReplaceAction',palette:'editor.action.quickCommand'};if(actions[name])editor.getAction(actions[name]).run();});
  document.getElementById('codeButton').onclick=()=>showPreview(false);
  document.getElementById('previewButton').onclick=()=>showPreview(true);
  document.getElementById('installButton').onclick=()=>host.installRequirements(activePath);
  document.getElementById('preview').onclick=e=>{const a=e.target.closest('a');if(a){e.preventDefault();host.requestExternalLink(a.href);}};
  editor.onMouseDown(e=>{if(!e.event.ctrlKey||!e.target.position||!editor.getModel())return;const line=editor.getModel().getLineContent(e.target.position.lineNumber);for(const match of line.matchAll(/https?:\/\/[^\s"'<>)}\]]+/g)){if(e.target.position.column>=match.index+1&&e.target.position.column<=match.index+match[0].length+1){host.requestExternalLink(match[0]);e.event.preventDefault();e.event.stopPropagation();return;}}});
  const context=editor.getContribution('editor.contrib.contextmenu');
  const getMenu=context._getMenuActions.bind(context);
  context._getMenuActions=(...args)=>getMenu(...args).filter(action=>action.id!=='editor.action.quickCommand');
  const keybindings=editor._standaloneKeybindingService;
  let registrations=[];
    function parseKey(sequence){
      const chords=sequence.split(',').map(s=>s.trim());if(chords.length===2)return monaco.KeyMod.chord(parseKey(chords[0]),parseKey(chords[1]));
      let key=0;const specials={left:'LeftArrow',right:'RightArrow',up:'UpArrow',down:'DownArrow','`':'Backquote',esc:'Escape',del:'Delete',return:'Enter',pgup:'PageUp',pgdown:'PageDown'};
      for(const raw of sequence.split('+')){const part=raw.trim(),lower=part.toLowerCase();if(lower==='ctrl')key|=monaco.KeyMod.CtrlCmd;else if(lower==='shift')key|=monaco.KeyMod.Shift;else if(lower==='alt')key|=monaco.KeyMod.Alt;else if(lower==='meta')key|=monaco.KeyMod.WinCtrl;else {const name=specials[lower]||(/^[0-9]$/.test(part)?'Digit'+part:part.length===1?'Key'+part.toUpperCase():Object.keys(monaco.KeyCode).find(k=>k.toLowerCase()===lower));key|=monaco.KeyCode[name]||0;}}return key;
    }
  function applySettings(){
    const settings=host.configuration;
    updateToolbarLabels();
    editor.updateOptions({fontSize:settings['editor.fontSize'],wordWrap:settings['editor.wordWrap']?'on':'off',minimap:{enabled:settings['editor.minimap']}});
    for(const model of models.values())model.updateOptions({tabSize:settings['editor.tabSize']});
      registrations.forEach(d=>d.dispose());registrations=[];
      const coreActions={find:'actions.find',replace:'editor.action.startFindReplaceAction',palette:'editor.action.quickCommand'};
      const allowed=new Set(['editor.action.clipboardCopyWithSyntaxHighlightingAction','editor.action.cursorUndo','editor.action.commentLine','editor.action.copyLinesDownAction','editor.action.moveLinesUpAction','editor.action.moveLinesDownAction','editor.action.deleteLines','editor.action.indentLines','editor.action.outdentLines','editor.action.clipboardCopyAction','editor.action.clipboardPasteAction','editor.action.clipboardCutAction','undo','redo','selectAll']);
      const defaults=keybindings._getResolver()._defaultKeybindings||[];
      const removed=new Set([...Object.values(coreActions),...defaults.filter(k=>k.command&&!allowed.has(k.command)&&!k.command.startsWith('educode.')).map(k=>k.command),...Object.keys(settings['editor.hotkeys']||{})]);
      registrations.push(keybindings.addDynamicKeybindings(Array.from(removed,id=>({command:'-'+id,keybinding:0}))));
    for(const [name,sequence] of Object.entries(host.shortcuts))if(sequence)registrations.push(keybindings.addDynamicKeybinding('educode.'+name,parseKey(sequence),()=>{flushChange();host.command(name);}));
    for(const [id,sequence] of Object.entries(settings['editor.hotkeys']||{})){
      const action=editor.getAction(id);if(action&&sequence)registrations.push(keybindings.addDynamicKeybinding(id,parseKey(sequence),()=>action.run()));
    }
      host.setEditorActions(JSON.stringify(editor.getSupportedActions().filter(action=>allowed.has(action.id)).map(action=>{const key=keybindings.lookupKeybinding(action.id);return {id:action.id,label:action.label,key:key?key.getLabel():''};})));
  }
  host.configurationChanged.connect(applySettings);applySettings();
  host.pluginsChanged.connect(refreshPluginLanguages);refreshPluginLanguages();
  host.editorReady();
 });
});
