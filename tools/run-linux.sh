#!/usr/bin/env bash
set -euo pipefail
root="$(dirname "$(dirname "$(readlink -f "$0")")")"
exec "$root/out/build/linux-release/EduCode/EduCode" "$@"
