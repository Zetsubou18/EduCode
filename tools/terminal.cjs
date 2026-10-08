// ConPTY stays in node-pty on Windows. Linux uses Python's standard-library
// pty helper so the Debian package does not ship an Ubuntu-specific native addon.
const readline = require('readline');
const path = require('path');
const win = process.platform === 'win32';
if (!win) {
  const {spawn} = require('child_process');
  const helper = path.join(__dirname, 'terminal-linux.py');
  const python = process.argv[3] || 'python3';
  const child = spawn(python, [helper, process.argv[2], python], {stdio: 'inherit'});
  child.on('error', error => {
    process.stdout.write(JSON.stringify({error:'Terminal: '+String(error)})+'\n');
    process.exit(1);
  });
  child.on('exit', code => process.exit(code === null ? 1 : code));
  process.on('SIGTERM', () => child.kill('SIGTERM'));
  return;
}
const pty = require('node-pty');
const env = {...process.env, VIRTUAL_ENV: path.dirname(path.dirname(process.argv[3]))};
const pathKey = Object.keys(env).find(key=>key.toLowerCase()==='path');
const systemPath = pathKey ? env[pathKey] : '';
for (const key of Object.keys(env)) if (key.toLowerCase()==='path') delete env[key];
env[win ? 'Path' : 'PATH'] = path.dirname(process.argv[3]) + path.delimiter + systemPath;
delete env.PYTHONHOME;
if(env.EDUCODE_PYTHON_TOOLS)env.PYTHONPATH=env.EDUCODE_PYTHON_TOOLS+(env.PYTHONPATH?path.delimiter+env.PYTHONPATH:"");
const shell = win ? (process.env.COMSPEC || 'cmd.exe') : (process.env.SHELL || '/bin/bash');
try {
  const session = pty.spawn(shell, win ? ['/d'] : ['-i'], {name:'xterm-256color',cols:100,rows:20,cwd:process.argv[2],env});
  session.onData(data => process.stdout.write(JSON.stringify({data})+'\n'));
  session.onExit(({exitCode}) => {process.stdout.write(JSON.stringify({data:`\r\nShell завершён · ${exitCode}\r\n`})+'\n');process.exit(0);});
  readline.createInterface({input:process.stdin}).on('line', line => {
    try {const msg=JSON.parse(line);if(msg.input!==undefined)session.write(msg.input);if(msg.cols)session.resize(msg.cols,msg.rows);}catch(e){process.stdout.write(JSON.stringify({error:String(e)})+'\n');}
  }).on('close',()=>{session.kill();process.exit(0);});
  process.on('SIGTERM',()=>{session.kill();process.exit(0);});
} catch(e) {process.stdout.write(JSON.stringify({error:String(e)})+'\n');process.exit(1);}
