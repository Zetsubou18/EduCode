param([ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$qt=Join-Path $root '.runtime/Qt/5.15.2/msvc2019_64'
if($env:EDUCODE_QT_ROOT){$qt=$env:EDUCODE_QT_ROOT}
$vswhere='C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$dev=Join-Path $vs 'Common7/Tools/Launch-VsDevShell.ps1'
& $dev -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
$env:VSLANG='1033'
$cmake=Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ninja=Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$linker=(Get-Command link.exe -ErrorAction Stop).Source
$build=Join-Path $root "out/build/educode-qt5-$($Configuration.ToLower())"
& $cmake -S $root -B $build -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration" "-DCMAKE_PREFIX_PATH=$qt" "-DQt5_DIR=$qt/lib/cmake/Qt5" "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_LINKER=$linker" '-DVCPKG_APPLOCAL_DEPS=OFF'
if($LASTEXITCODE -ne 0){throw 'CMake configuration failed'}
& $cmake --build $build --parallel 6
if($LASTEXITCODE -ne 0){throw 'Build failed'}
$deployMode=if($Configuration -eq 'Debug'){'--debug'}else{'--release'}
& (Join-Path $qt 'bin/windeployqt.exe') $deployMode --force --qmldir (Join-Path $root 'qml') --no-translations (Join-Path $build 'EduCode/EduCode.exe')
if($LASTEXITCODE -ne 0){throw 'Qt deployment failed'}
Copy-Item -LiteralPath (Join-Path $qt "bin/Qt5Test.dll") -Destination (Join-Path $build "EduCode") -Force
Write-Output "Ready: $build/EduCode/EduCode.exe"
