'use strict';
require.config({'vs/nls':{availableLanguages:{'*':new URLSearchParams(location.search).get('lang')==='en'?'en':'ru'}},paths:{vs:'../node_modules/monaco-editor/min/vs'}});
let host, editor, secondEditor, activePath='', splitPath='', serverReady=false, requestId=0, changing=false, changeTimer, fstringDecorations=[];
const openedLsp=new Set();
const models=new Map(), versions=new Map(), requests=new Map(), diagnostics=new Map();
const pythonBuiltins=new Set(['abs','all','any','bin','bool','breakpoint','bytearray','bytes','callable','chr','classmethod','compile','complex','delattr','dict','dir','divmod','enumerate','eval','exec','filter','float','format','frozenset','getattr','globals','hasattr','hash','help','hex','id','input','int','isinstance','issubclass','iter','len','list','locals','map','max','memoryview','min','next','object','oct','open','ord','pow','print','property','range','repr','reversed','round','set','setattr','slice','sorted','staticmethod','str','sum','super','tuple','type','vars','zip','__import__']);
const pluginLanguages=new Map(), registeredLanguages=new Set();
const uri=p=>monaco.Uri.file(p).toString();
const pathOf=u=>monaco.Uri.parse(u).fsPath.replace(/\\/g,'/');
const position=p=>({line:p.lineNumber-1,character:p.column-1});
const range=r=>({startLineNumber:r.start.line+1,startColumn:r.start.character+1,endLineNumber:r.end.line+1,endColumn:r.end.character+1});
const isPython=p=>/\.pyi?$/i.test(p);
function pythonDefinitions(model){
 const definitions=new Map();
 for(let line=1;line<=model.getLineCount();line++){
  const text=model.getLineContent(line),match=text.match(/^\s*(?:async\s+)?(?:def|class)\s+([A-Za-z_]\w*)\s*\(([^)]*)\)/);
  if(!match)continue;
  const column=text.indexOf(match[1])+1;
  const params=match[2].split(',').map(p=>p.trim().replace(/^\*{0,2}/,'').split(/\s*[:=]\s*/)[0]).filter(p=>p&&p!=='self'&&p!=='cls');
  definitions.set(match[1],{line,column,params});
 }
 return definitions;
}
function argumentOffsets(text,start){let depth=0,quote='',escaped=false,result=[start];for(let i=start;i<text.length;i++){const c=text[i];if(quote){if(escaped)escaped=false;else if(c==='\\')escaped=true;else if(c===quote)quote='';continue;}if(c==='"'||c==="'"){quote=c;continue;}if(c==='('||c==='['||c==='{')depth++;else if(c===')'||c===']'||c==='}'){if(depth===0)break;depth--;}else if(c===','&&depth===0)result.push(i+1);}return result;}
let referenceSelection=0,referenceItems=[];
const pyCharmWidgetStyles=document.createElement('style');
pyCharmWidgetStyles.textContent=`
.monaco-editor .suggest-widget,.monaco-editor .monaco-hover{border-radius:8px!important;box-shadow:0 10px 28px #0008!important;border-color:#43454a!important}
.monaco-editor .suggest-widget .monaco-list-row{border-radius:4px;margin:0 4px;width:calc(100% - 8px)!important}
.monaco-editor .suggest-widget .monaco-list-row.focused{background:#2f4b72!important}.monaco-editor .suggest-widget .details-label{color:#9da0a8!important}
.monaco-editor .monaco-hover{max-width:min(620px,calc(100vw - 24px))!important;max-height:min(480px,calc(100vh - 24px))!important}
.monaco-editor .monaco-hover .monaco-scrollable-element{max-height:450px!important}.monaco-editor .monaco-hover .hover-row{padding:8px 10px!important}
.monaco-editor .monaco-hover code{font-family:'Cascadia Code',Consolas,monospace;color:#bcbec4}
.monaco-editor .fstring-brace{color:#cf8e6d!important;font-weight:600}.monaco-editor .py-function-declaration{color:#56a8f5!important}
.monaco-editor .py-decorator{color:#b3ae60!important}.monaco-editor .py-builtin{color:#8888c6!important}.monaco-editor .py-self{color:#94558d!important}
.monaco-editor .py-special-name{color:#b200b2!important}.monaco-editor .py-named-argument{color:#aa4926!important}.monaco-editor .py-binary-string{color:#a5c261!important}
.monaco-editor .py-docstring{color:#5f826b!important;font-style:italic}.monaco-editor .py-doc-tag{color:#67a37c!important;font-style:italic}`;
document.head.appendChild(pyCharmWidgetStyles);
function hideReferences(){const popup=document.getElementById('referencesPopup');if(popup)popup.style.display='none';referenceItems=[];}
function referenceLabel(count){const tail=count%100;if(tail>=11&&tail<=14)return 'ссылок';const last=count%10;return last===1?'ссылка':last>=2&&last<=4?'ссылки':'ссылок';}
function referencePreview(item){const path=pathOf(item.uri),model=models.get(path),line=item.range.startLineNumber;return model&&line<=model.getLineCount()?model.getLineContent(line).trim():'';}
function openReference(item){if(!item||!host)return;const p=editor.getPosition()||{lineNumber:1,column:1};hideReferences();host.navigate(pathOf(item.uri),item.range.startLineNumber,item.range.startColumn,activePath,p.lineNumber,p.column);}
function selectReference(index){const buttons=[...document.querySelectorAll('.referenceItem')];if(!buttons.length)return;referenceSelection=(index+buttons.length)%buttons.length;buttons.forEach((button,i)=>button.classList.toggle('selected',i===referenceSelection));buttons[referenceSelection].scrollIntoView({block:'nearest'});}
function showReferences(payload){
 const popup=document.getElementById('referencesPopup'),list=document.getElementById('referencesList'),refs=(payload&&payload.references)||[];
 referenceItems=refs;referenceSelection=0;document.getElementById('referencesTitle').textContent=(payload&&payload.kind||'Function')+' '+(payload&&payload.name||'');document.getElementById('referencesCount').textContent=refs.length+' '+referenceLabel(refs.length);list.textContent='';
 if(!refs.length){const empty=document.createElement('div');empty.className='referencesEmpty';empty.textContent='Использования не найдены';list.appendChild(empty);}else refs.forEach((item,index)=>{const button=document.createElement('button'),path=pathOf(item.uri),file=path.split('/').pop();button.className='referenceItem'+(index===0?' selected':'');button.innerHTML='<span class="referenceIcon">Py</span><span class="referenceFile"></span><span class="referenceCode"></span>';button.children[1].textContent=file+'  '+item.range.startLineNumber;button.children[2].textContent=referencePreview(item);button.title=path+':'+item.range.startLineNumber;button.onmouseenter=()=>selectReference(index);button.onclick=()=>openReference(item);list.appendChild(button);});
 popup.style.display='block';const anchor=editor.getScrolledVisiblePosition((payload&&payload.position)||editor.getPosition()),rect=document.getElementById('editor').getBoundingClientRect();let left=rect.left+Math.max(8,anchor?anchor.left:40),top=rect.top+(anchor?anchor.top+anchor.height+8:50);const width=Math.min(440,innerWidth-20),estimated=Math.min(360,75+refs.length*30);left=Math.max(10,Math.min(left,innerWidth-width-10));if(top+estimated>innerHeight-10)top=Math.max(10,rect.top+(anchor?anchor.top:50)-estimated-8);popup.style.left=left+'px';popup.style.top=top+'px';
}
function maskPythonLine(text){
 const out=text.split('');let quote='',escaped=false;
 for(let i=0;i<text.length;i++){const c=text[i];if(quote){out[i]=' ';if(escaped)escaped=false;else if(c==='\\')escaped=true;else if(c===quote)quote='';continue;}if(c==='#'){for(let j=i;j<out.length;j++)out[j]=' ';break;}if(c==='"'||c==="'"){quote=c;out[i]=' ';}}
 return out.join('');
}
function pushMatches(next,line,text,regex,className,group=0,base=0){
 for(const match of text.matchAll(regex)){const value=match[group]||match[0],offset=group?match[0].indexOf(value):0,start=base+match.index+offset;next.push({range:new monaco.Range(line,start+1,line,start+value.length+1),options:{inlineClassName:className}});}
}
function updateFStringDecorations(){
 if(!editor||!editor.getModel()||!isPython(editor.getModel().uri.fsPath)){fstringDecorations=editor?editor.deltaDecorations(fstringDecorations,[]):[];return;}
 const model=editor.getModel(),next=[];let docQuote='';
 for(let line=1;line<=model.getLineCount();line++){
  const text=model.getLineContent(line);let quote='',inExpression=false,depth=0,escaped=false;
  let docStart=-1,docEnd=-1;
  if(docQuote){docStart=0;docEnd=text.indexOf(docQuote);if(docEnd>=0){docEnd+=docQuote.length;docQuote='';}else docEnd=text.length;}
  else {const opening=text.match(/(?:[rubf]*)?("""|''')/i);if(opening){const marker=opening[1];docStart=opening.index;const closing=text.indexOf(marker,docStart+opening[0].length);if(closing>=0)docEnd=closing+marker.length;else{docEnd=text.length;docQuote=marker;}}}
  if(docStart>=0){next.push({range:new monaco.Range(line,docStart+1,line,docEnd+1),options:{inlineClassName:'py-docstring'}});pushMatches(next,line,text.slice(docStart,docEnd),/@[A-Za-z_]\w*/g,'py-doc-tag',0,docStart);}
  for(let i=0;i<text.length;i++){
   if(!quote){const m=text.slice(i).match(/^(?:[rub]*)f(?:[rub]*)('''|\"\"\"|'|\")/i);if(m){quote=m[1];i+=m[0].length-1;}continue;}
   if(!inExpression&&text.startsWith(quote,i)){const length=quote.length;quote='';i+=length-1;continue;}
   const c=text[i];if(escaped){escaped=false;continue;}if(c==='\\'){escaped=true;continue;}
   if(c==='{'&&text[i+1]!=='{'){inExpression=true;depth++;next.push({range:new monaco.Range(line,i+1,line,i+2),options:{inlineClassName:'fstring-brace'}});}
   else if(c==='}'&&text[i+1]!=='}'&&inExpression){next.push({range:new monaco.Range(line,i+1,line,i+2),options:{inlineClassName:'fstring-brace'}});if(--depth<=0){depth=0;inExpression=false;}}
  }
  if(docStart>=0&&docEnd===text.length&&text.slice(0,docStart).trim()==='')continue;
  const masked=maskPythonLine(text),definition=masked.match(/\bdef\s+([A-Za-z_]\w*)/);
  if(definition){const start=definition.index+definition[0].lastIndexOf(definition[1]);next.push({range:new monaco.Range(line,start+1,line,start+definition[1].length+1),options:{inlineClassName:'py-function-declaration'}});}
  pushMatches(next,line,masked,/^\s*@(?:[A-Za-z_]\w*)(?:\.[A-Za-z_]\w*)*/g,'py-decorator');
  pushMatches(next,line,masked,/\bself\b/g,'py-self');
  pushMatches(next,line,masked,/\b__[A-Za-z_]\w*__\b/g,'py-special-name');
  pushMatches(next,line,masked,/\b[A-Za-z_]\w*\b/g,'py-builtin');
  for(let i=next.length-1;i>=0;i--){const item=next[i],value=model.getValueInRange(item.range);if(item.options.inlineClassName==='py-builtin'&&!pythonBuiltins.has(value))next.splice(i,1);}
  if(!/^\s*(?:async\s+)?def\b/.test(masked)&&masked.includes('('))pushMatches(next,line,masked,/\b([A-Za-z_]\w*)\s*(?==)/g,'py-named-argument',1);
  pushMatches(next,line,text,/\bb(?:r|R)?(["'])(?:\\.|.)*?\1/g,'py-binary-string');
 }
 fstringDecorations=editor.deltaDecorations(fstringDecorations,next);
}
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
  if(!model){model=monaco.editor.createModel(text,language(path),monaco.Uri.file(path));models.set(path,model);model.onDidChangeContent(()=>{if(changing)return;host.editDocument(path,model.getValue());if(path===activePath||path===splitPath){clearTimeout(changeTimer);changeTimer=setTimeout(()=>flushChange(path),180);}if(path===activePath)updateFStringDecorations();if(document.getElementById('preview').style.display==='block'&&path===activePath)showPreview(true);});}
  else if(model.getValue()!==text){
    changing=true;
    try{model.pushEditOperations([],[{range:model.getFullModelRange(),text}],()=>null);textChanged=true;}finally{changing=false;}
  }
  const old=editor.getModel();if(old&&activePath)old._eduView=editor.saveViewState();const switched=activePath!==path;activePath=path;editor.setModel(model);if(model._eduView)editor.restoreViewState(model._eduView);
  if(switched)openLsp();else if(textChanged)flushChange();if(line>0){column=Math.max(1,column);editor.setPosition({lineNumber:line,column});editor.revealPositionInCenter({lineNumber:line,column});}
  if(host&&models.has(path))models.get(path).updateOptions({tabSize:host.configuration["editor.tabSize"]});
  if(!isPython(path))diagnostics.delete(uri(path));reportProblems();updateToolbar();updateFStringDecorations();editor.focus();
}
require(['vs/editor/editor.main'],()=>{
 monaco.languages.register({id:'requirements'});monaco.languages.setMonarchTokensProvider('requirements',{tokenizer:{root:[[/^\s*#.*/, 'comment'],[/^[A-Za-z0-9_.-]+/, 'type.identifier'],[/(==|>=|<=|~=|!=|>|<)/,'keyword'],[/\d+(?:\.\d+)*/, 'number']]}});
 monaco.languages.register({id:'env'});monaco.languages.setMonarchTokensProvider('env',{tokenizer:{root:[[/^\s*#.*/, 'comment'],[/^[A-Za-z_][A-Za-z0-9_]*(?==)/,'type.identifier'],[/=.*/, 'string']]}});
 monaco.languages.register({id:'gitignore'});monaco.languages.setMonarchTokensProvider('gitignore',{tokenizer:{root:[[/^\s*#.*/, 'comment'],[/^!.*$/, 'keyword'],[/[^\s]+/, 'string']]}});
 monaco.editor.defineTheme('educode',{base:'vs-dark',inherit:true,rules:[
  {token:'',foreground:'BCBEC4'},{token:'identifier',foreground:'BCBEC4'},{token:'delimiter',foreground:'BCBEC4'},{token:'operator',foreground:'BCBEC4'},
  {token:'keyword',foreground:'CF8E6D'},{token:'number',foreground:'2AACB8'},{token:'string',foreground:'6AAB73'},{token:'string.escape',foreground:'CF8E6D'},{token:'string.escape.invalid',foreground:'FA6675'},
  {token:'comment',foreground:'747A80'},{token:'comment.doc',foreground:'5F826B',fontStyle:'italic'},{token:'tag',foreground:'67A37C'},
  {token:'type.identifier',foreground:'BCBEC4'},{token:'function',foreground:'56A8F5'},{token:'function.declaration',foreground:'56A8F5'},{token:'decorator',foreground:'B3AE60'},
  {token:'variable.predefined',foreground:'8888C6'},{token:'variable.self',foreground:'94558D'},{token:'variable.special',foreground:'B200B2'}
 ],colors:{'editor.background':'#1E1F22','editor.foreground':'#BCBEC4','editorLineNumber.foreground':'#5A5D63','editorLineNumber.activeForeground':'#A4A7AD','editor.lineHighlightBackground':'#24262B','editor.selectionBackground':'#2E436E','editor.inactiveSelectionBackground':'#29364F','editorCursor.foreground':'#CED0D6','editorBracketHighlight.foreground1':'#BCBEC4','editorBracketHighlight.foreground2':'#BCBEC4','editorBracketHighlight.foreground3':'#BCBEC4','editorWidget.background':'#2B2D30','editorWidget.border':'#43454A','editorSuggestWidget.background':'#2B2D30','editorSuggestWidget.border':'#43454A','editorSuggestWidget.foreground':'#BCBEC4','editorSuggestWidget.selectedBackground':'#2F4B72','editorSuggestWidget.highlightForeground':'#56A8F5','editorHoverWidget.background':'#2B2D30','editorHoverWidget.border':'#43454A','editorHoverWidget.foreground':'#BCBEC4','menu.background':'#2B2D30','menu.foreground':'#BCBEC4','menu.border':'#43454A','menu.selectionBackground':'#2F4B72','menu.selectionForeground':'#FFFFFF','focusBorder':'#5C7EAB'}});
 editor=monaco.editor.create(document.getElementById('editor'),{theme:'educode',fontFamily:'Cascadia Code, Consolas, monospace',fontSize:14,lineHeight:23,minimap:{enabled:false},padding:{top:18,bottom:16},scrollBeyondLastLine:false,automaticLayout:true,tabSize:4,insertSpaces:true,renderLineHighlight:'line',overviewRulerBorder:false,quickSuggestions:{other:true,comments:false,strings:true},quickSuggestionsDelay:180,suggestOnTriggerCharacters:true,wordBasedSuggestions:false,suggest:{showWords:false,preview:true,showStatusBar:false,selectionMode:'always',detailsVisible:true},hover:{delay:500,above:true},parameterHints:{enabled:true},codeLens:true,inlayHints:{enabled:true},bracketPairColorization:{enabled:false},fixedOverflowWidgets:false,smoothScrolling:true,mouseWheelZoom:true});
 monaco.languages.registerCompletionItemProvider('python',{triggerCharacters:['.'],provideCompletionItems:async(model,p,ctx,token)=>{
   const result=await request('textDocument/completion',{textDocument:{uri:model.uri.toString()},position:position(p),context:{triggerKind:ctx.triggerKind,triggerCharacter:ctx.triggerCharacter}},token);
   if(token.isCancellationRequested)return {suggestions:[]};const word=model.getWordUntilPosition(p);const items=Array.isArray(result)?result:(result&&result.items)||[];
   const kinds={1:monaco.languages.CompletionItemKind.Text,2:monaco.languages.CompletionItemKind.Method,3:monaco.languages.CompletionItemKind.Function,4:monaco.languages.CompletionItemKind.Constructor,5:monaco.languages.CompletionItemKind.Field,6:monaco.languages.CompletionItemKind.Variable,7:monaco.languages.CompletionItemKind.Class,9:monaco.languages.CompletionItemKind.Module,10:monaco.languages.CompletionItemKind.Property,14:monaco.languages.CompletionItemKind.Keyword,21:monaco.languages.CompletionItemKind.Constant};
   return {suggestions:items.map(i=>({label:i.label,kind:kinds[i.kind]||monaco.languages.CompletionItemKind.Variable,detail:i.detail,documentation:typeof i.documentation==='string'?i.documentation:i.documentation&&{value:i.documentation.value},insertText:i.textEdit?i.textEdit.newText:i.insertText||i.label,insertTextRules:i.insertTextFormat===2?monaco.languages.CompletionItemInsertTextRule.InsertAsSnippet:undefined,range:i.textEdit&&i.textEdit.range?range(i.textEdit.range):{startLineNumber:p.lineNumber,endLineNumber:p.lineNumber,startColumn:word.startColumn,endColumn:word.endColumn},additionalTextEdits:(i.additionalTextEdits||[]).map(e=>({range:range(e.range),text:e.newText})),sortText:i.sortText,filterText:i.filterText,_lsp:i})),incomplete:!!(result&&result.isIncomplete)};
 },resolveCompletionItem:async(item,token)=>{const i=await request('completionItem/resolve',item._lsp,token);if(i){item.detail=i.detail;item.documentation=typeof i.documentation==='string'?i.documentation:i.documentation&&{value:i.documentation.value};item.additionalTextEdits=(i.additionalTextEdits||[]).map(e=>({range:range(e.range),text:e.newText}));}return item;}});
 monaco.languages.registerHoverProvider('python',{provideHover:async(model,p,token)=>{const r=await request('textDocument/hover',{textDocument:{uri:model.uri.toString()},position:position(p)},token);if(!r)return null;const contents=(Array.isArray(r.contents)?r.contents:[r.contents]).filter(Boolean).map(c=>({value:typeof c==='string'?c:c.language?'```'+c.language+'\n'+c.value+'\n```':c.value}));return {contents,range:r.range&&range(r.range)};}});
 monaco.languages.registerSignatureHelpProvider('python',{signatureHelpTriggerCharacters:['(',','],signatureHelpRetriggerCharacters:[',',')'],provideSignatureHelp:async(model,p,token)=>{const r=await request('textDocument/signatureHelp',{textDocument:{uri:model.uri.toString()},position:position(p)},token);if(!r)return null;return {value:{activeSignature:r.activeSignature||0,activeParameter:r.activeParameter||0,signatures:r.signatures.map(s=>({...s,documentation:typeof s.documentation==='string'?s.documentation:s.documentation&&{value:s.documentation.value},parameters:s.parameters||[]}))},dispose(){}};}});
 monaco.languages.registerDefinitionProvider('python',{provideDefinition:async(model,p,token)=>{const r=await request('textDocument/definition',{textDocument:{uri:model.uri.toString()},position:position(p)},token);return (Array.isArray(r)?r:r?[r]:[]).map(d=>({uri:monaco.Uri.parse(d.uri||d.targetUri),range:range(d.range||d.targetSelectionRange)}));}});
 monaco.languages.registerInlayHintsProvider('python',{provideInlayHints(model,area){
   const hints=[],defs=pythonDefinitions(model);
   for(let line=area.startLineNumber;line<=area.endLineNumber;line++){
    const text=model.getLineContent(line);for(const call of text.matchAll(/\b([A-Za-z_]\w*)\s*\(/g)){
     const def=defs.get(call[1]);if(!def||line===def.line)continue;const start=call.index+call[0].length,offsets=argumentOffsets(text,start);
     offsets.slice(0,def.params.length).forEach((offset,index)=>{const tail=text.slice(offset).match(/^\s*/)[0].length,pos=offset+tail;if(!new RegExp('^'+def.params[index]+'\\s*=').test(text.slice(pos)))hints.push({kind:monaco.languages.InlayHintKind.Parameter,position:{lineNumber:line,column:pos+1},label:def.params[index]+':',paddingRight:true});});
    }
   }
   return {hints,dispose(){}};
 }});
 monaco.languages.registerCodeLensProvider('python',{provideCodeLenses:async(model,token)=>{
   const entries=[...pythonDefinitions(model)];const lenses=await Promise.all(entries.map(async([name,def])=>{const r=await request('textDocument/references',{textDocument:{uri:model.uri.toString()},position:{line:def.line-1,character:def.column-1},context:{includeDeclaration:false}},token);const refs=(Array.isArray(r)?r:[]).map(x=>({uri:x.uri,range:range(x.range)}));return {range:new monaco.Range(def.line,def.column,def.line,def.column+name.length),command:{id:'educode.showReferences',title:refs.length+' '+referenceLabel(refs.length),arguments:[{name,kind:'Function',position:{lineNumber:def.line,column:def.column},references:refs}]}};}));return {lenses:token.isCancellationRequested?[]:lenses,dispose(){}};
 }});
 monaco.editor.registerCommand('educode.showReferences',(_accessor,payload)=>showReferences(payload));
 monaco.languages.registerHoverProvider('python',{provideHover:(model,p)=>{const word=model.getWordAtPosition(p);if(!word)return null;for(let line=1;line<=model.getLineCount();line++){const m=model.getLineContent(line).match(new RegExp('^\\s*'+word.word+'\\s*=\\s*\\[([^\\]]*)\\]'));if(!m)continue;const types=new Set(m[1].split(',').map(v=>v.trim()).filter(Boolean).map(v=>/^[rubf]*[\"']/.test(v)?'str':/^(True|False)$/.test(v)?'bool':/^-?\d+(?:\.\d+)?$/.test(v)?(v.includes('.')?'float':'int'):'Unknown'));if(types.size)return {contents:[{value:'`'+word.word+': list['+[...types].join(' | ')+']` *(EduCode inference)*'}]};}return null;}});
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
  editor.onMouseDown(e=>{hideReferences();if(!e.event.ctrlKey||!e.target.position||!editor.getModel())return;const line=editor.getModel().getLineContent(e.target.position.lineNumber);for(const match of line.matchAll(/https?:\/\/[^\s"'<>)}\]]+/g)){if(e.target.position.column>=match.index+1&&e.target.position.column<=match.index+match[0].length+1){host.requestExternalLink(match[0]);e.event.preventDefault();e.event.stopPropagation();return;}}});
  editor.onDidScrollChange(hideReferences);
  document.getElementById('referencesClose').onclick=hideReferences;
  document.addEventListener('keydown',event=>{if(document.getElementById('referencesPopup').style.display!=='block')return;if(event.key==='Escape'){hideReferences();event.preventDefault();}else if(event.key==='ArrowDown'){selectReference(referenceSelection+1);event.preventDefault();}else if(event.key==='ArrowUp'){selectReference(referenceSelection-1);event.preventDefault();}else if(event.key==='Enter'){openReference(referenceItems[referenceSelection]);event.preventDefault();}});
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
      // Preserve Monaco's editing/navigation defaults. Removing the complete default keymap broke
      // Tab/Shift+Tab, selection deletion and Backspace-at-line-start.
      const removed=new Set([...Object.values(coreActions),...Object.keys(settings['editor.hotkeys']||{})]);
      registrations.push(keybindings.addDynamicKeybindings(Array.from(removed,id=>({command:'-'+id,keybinding:0}))));
    for(const [name,sequence] of Object.entries(host.shortcuts))if(sequence)registrations.push(keybindings.addDynamicKeybinding('educode.'+name,parseKey(sequence),()=>{flushChange();host.command(name);}));
    for(const [id,sequence] of Object.entries(settings['editor.hotkeys']||{})){
      const action=editor.getAction(id);if(action&&sequence)registrations.push(keybindings.addDynamicKeybinding(id,parseKey(sequence),()=>action.run()));
    }
      host.setEditorActions(JSON.stringify(editor.getSupportedActions().filter(action=>allowed.has(action.id)).map(action=>{const appCommand=Object.keys(coreActions).find(name=>coreActions[name]===action.id),key=appCommand?host.shortcuts[appCommand]:keybindings.lookupKeybinding(action.id);return {id:action.id,label:action.label,key:typeof key==='string'?key:key?key.getLabel():''};})));
  }
  host.configurationChanged.connect(applySettings);applySettings();
  host.pluginsChanged.connect(refreshPluginLanguages);refreshPluginLanguages();
  host.editorReady();
 });
});
