param([string]$Python='python',[string]$Node='C:/Program Files/nodejs/node.exe')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    if(-not (Test-Path '.runtime/Qt/5.15.2/msvc2019_64/bin/qmake.exe')){
        & $Python -m pip install aqtinstall
        if($LASTEXITCODE -ne 0){throw 'aqtinstall failed'}
        & $Python -m aqt install-qt windows desktop 5.15.2 win64_msvc2019_64 -O .runtime/Qt -m qtwebengine
        if($LASTEXITCODE -ne 0){throw 'Qt installation failed'}
    }
    $npm=Join-Path (Split-Path $Node -Parent) 'npm.cmd'
    & $npm ci --no-audit --no-fund
    if($LASTEXITCODE -ne 0){throw 'npm ci failed'}
} finally {Pop-Location}
