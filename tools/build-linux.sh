#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B out/build/linux-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/linux-release --parallel "${EDUCODE_BUILD_JOBS:-2}"
EDUCODE_TEST_PYTHON="$(command -v python3)" ctest --test-dir out/build/linux-release --output-on-failure
printf '%s\n' "Run: bash tools/run-linux.sh"
