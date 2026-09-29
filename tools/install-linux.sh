#!/usr/bin/env bash
set -euo pipefail
root="$(dirname "$(dirname "$(readlink -f "$0")")")"
binary="$root/out/build/linux-release/EduCode/EduCode"
[[ -x "$binary" ]] || { echo "Build EduCode first." >&2; exit 1; }
if [[ -e /usr/local/bin/EduCode || -L /usr/local/bin/EduCode ]]; then
 [[ "$(readlink -f /usr/local/bin/EduCode)" == "$binary" ]] || { echo "Existing /usr/local/bin/EduCode belongs to another installation." >&2; exit 1; }
else
 sudo ln -sfn "$binary" /usr/local/bin/EduCode
fi
mkdir -p "$HOME/.local/share/applications"
cat > "$HOME/.local/share/applications/EduCode.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=EduCode
Comment=Python IDE
Exec="$binary" %F
Icon=$root/assets/app_logo.png
Terminal=false
Categories=Development;IDE;
MimeType=text/x-python;application/json;text/plain;text/markdown;inode/directory;
DESKTOP
printf '%s\n' 'Ready: EduCode, EduCode test.py, EduCode /path/to/project'
