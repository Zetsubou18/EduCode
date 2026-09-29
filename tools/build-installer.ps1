param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build.ps1') -Configuration Release; if($LASTEXITCODE -ne 0){throw 'EduCode build failed'}}
$qt=Join-Path $root '.runtime/Qt/5.15.2/msvc2019_64'
if($env:EDUCODE_QT_ROOT){$qt=$env:EDUCODE_QT_ROOT}
$build=Join-Path $root 'out/build/educode-qt5-release/EduCode'
$stage=Join-Path $root 'out/installer/runtime'
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item -LiteralPath (Join-Path $build 'SetupGui.exe') -Destination $stage -Force
& (Join-Path $qt 'bin/windeployqt.exe') --release --force --no-translations (Join-Path $stage 'SetupGui.exe') | Out-Null
if($LASTEXITCODE -ne 0){throw 'Qt Widgets setup deployment failed'}
$crt=Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\VC\Redist\MSVC\*\x64\Microsoft.VC*.CRT' -Directory -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
if(-not $crt){throw 'MSVC x64 CRT redistributable was not found'}
Get-ChildItem -LiteralPath $crt -Filter '*.dll' | Copy-Item -Destination $stage -Force
$python=(Get-Command python -ErrorAction Stop).Source
& $python (Join-Path $PSScriptRoot 'build-installer.py') $root
if($LASTEXITCODE -ne 0){throw 'Installer packaging failed'}
