#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
built="$root/out/build/linux-release/EduCode"
[[ -x "$built/EduCode" && -d "$built/node_modules" ]] || { echo 'Build Linux release first: bash tools/build-linux.sh' >&2; exit 1; }
stage="$(mktemp -d)"
trap 'rm -rf -- "$stage"' EXIT
app="$stage/opt/educode"
mkdir -p "$app" "$stage/DEBIAN" "$stage/usr/bin" "$stage/usr/share/applications" "$stage/usr/share/icons/hicolor/256x256/apps"
install -m 755 "$built/EduCode" "$app/EduCode"
for part in qml web assets tools node_modules docs plugins; do
  cp -a "$built/$part" "$app/$part"
done
cp "$root/README.md" "$app/README.md"
find "$app" -type d -name __pycache__ -prune -exec rm -rf -- {} +
cat > "$stage/usr/bin/EduCode" <<'WRAPPER'
#!/usr/bin/env sh
exec /opt/educode/EduCode "$@"
WRAPPER
chmod 755 "$stage/usr/bin/EduCode"
cat > "$stage/usr/share/applications/educode.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=EduCode
Comment=Python IDE with optional plugins
Exec=EduCode %F
Icon=educode
Terminal=false
Categories=Development;IDE;
MimeType=text/x-python;application/json;text/plain;text/markdown;inode/directory;
DESKTOP
install -m 644 "$root/assets/app_logo.png" "$stage/usr/share/icons/hicolor/256x256/apps/educode.png"
size="$(du -sk "$app" | cut -f1)"
arch="$(dpkg --print-architecture)"
cat > "$stage/DEBIAN/control" <<CONTROL
Package: educode
Version: 0.2.0
Section: devel
Priority: optional
Architecture: $arch
Maintainer: Zetsubou <zetsubou@users.noreply.github.com>
Installed-Size: $size
Depends: nodejs, python3 (>= 3.10), python3-venv, libqt5webenginecore5, qml-module-qtwebengine, qml-module-qtwebchannel, qml-module-qtquick-controls2, qml-module-qtquick-layouts, qml-module-qtquick-dialogs, qml-module-qtquick2, qml-module-qtgraphicaleffects
Homepage: https://github.com/Zetsubou18/EduCode
Description: EduCode Python IDE
 Desktop Python IDE with Monaco editor, Pyright diagnostics,
 terminal, and optional Python plugins.
CONTROL
mkdir -p "$root/out/release"
package="$root/out/release/educode_0.2.0_${arch}.deb"
dpkg-deb --root-owner-group --build "$stage" "$package"
echo "$package"
