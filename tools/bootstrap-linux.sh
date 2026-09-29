#!/usr/bin/env bash
set -euo pipefail
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build nodejs npm python3 python3-venv python3-dev qtbase5-dev qtdeclarative5-dev qtquickcontrols2-5-dev qtwebengine5-dev libqt5webchannel5-dev qml-module-qtwebengine qml-module-qtwebchannel qml-module-qtquick2 qml-module-qtquick-controls qml-module-qtgraphicaleffects qml-module-qtquick-controls2 qml-module-qtquick-layouts qml-module-qtquick-window2 qml-module-qtquick-dialogs qml-module-qt-labs-platform
cd "$(dirname "$0")/.."
npm ci
