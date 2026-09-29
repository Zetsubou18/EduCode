param()
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$source=Join-Path $root 'out/build/educode-qt5-release/EduCode'
$destination=Join-Path $root 'out/app/EduCode'
New-Item -ItemType Directory -Force $destination | Out-Null
# Incremental copy preserves the existing bundle and avoids rewriting thousands of unchanged stubs.
& robocopy $source $destination /E /XO /MT:8 /R:1 /W:1 /NFL /NDL /NJH /NJS /NP /XD CMakeFiles EduCode_autogen CoreTests_autogen /XF CoreTests.exe TeacherTests.exe Qt5Test.dll cmake_install.cmake CTestTestfile.cmake | Out-Null
if($LASTEXITCODE -ge 8){throw "Runtime copy failed: robocopy exit code $LASTEXITCODE"}
Copy-Item -LiteralPath (Join-Path $root 'README.md'),(Join-Path $root 'THIRD_PARTY_NOTICES.md') -Destination $destination -Force
Write-Output "Portable application: $destination/EduCode.exe"

