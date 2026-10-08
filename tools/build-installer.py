"""Make one offline Windows EXE: Win32 launcher + Qt Widgets setup + app archive."""
from pathlib import Path
import shutil
import struct
import sys
import zipfile

root = Path(sys.argv[1])
build = root / 'out/build/educode-qt5-release/EduCode'
runtime = root / 'out/installer/runtime'
release = root / 'out/release'
release.mkdir(parents=True, exist_ok=True)
launcher = (build / 'SetupLauncher.exe').read_bytes()


def archive(directory, target, excluded=()):
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED, compresslevel=6,
                         allowZip64=True) as result:
        for source in sorted(directory.rglob('*')):
            if source.is_file() and source.relative_to(directory).as_posix() not in excluded:
                result.write(source, source.relative_to(directory).as_posix())


def bootstrap(target, archive_path):
    with target.open('wb') as output, archive_path.open('rb') as payload:
        output.write(launcher)
        shutil.copyfileobj(payload, output, 1024 * 1024)
        output.write(struct.pack('<Q', archive_path.stat().st_size))
        output.write(b'EDUSETUP')


runtime_zip = root / 'out/installer/runtime.zip'
archive(runtime, runtime_zip, {'EduCodeUninstall.exe', 'payload.zip', 'vc_redist.x64.exe'})
uninstaller = runtime / 'EduCodeUninstall.exe'
bootstrap(uninstaller, runtime_zip)

payload_zip = runtime / 'payload.zip'
archive(build, payload_zip, {
    'CoreTests.exe', 'TeacherTests.exe', 'Qt5Test.dll', 'SetupGui.exe', 'SetupLauncher.exe'
})
bundle_zip = root / 'out/installer/bundle.zip'
with zipfile.ZipFile(bundle_zip, 'w', zipfile.ZIP_DEFLATED, compresslevel=6, allowZip64=True) as result:
    for source in sorted(runtime.rglob('*')):
        if source.is_file() and source.name != 'vc_redist.x64.exe':
            method = zipfile.ZIP_STORED if source == payload_zip else zipfile.ZIP_DEFLATED
            result.write(source, source.relative_to(runtime).as_posix(), compress_type=method)
target = release / 'EduCodeSetup-0.3.0-win64.exe'
bootstrap(target, bundle_zip)
print(f'{target} ({target.stat().st_size / 1024 / 1024:.1f} MiB)')
