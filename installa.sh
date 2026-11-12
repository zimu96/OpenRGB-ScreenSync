#!/usr/bin/env bash
# Compila e installa il plugin Screen Sync in ~/.config/OpenRGB/plugins
set -e
cd "$(dirname "$0")"
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release >/dev/null
make -j"$(nproc)"
mkdir -p "$HOME/.config/OpenRGB/plugins"
mkdir -p ../DA-CREARE-Release/1-Linux && cp -f ../libScreenSyncPlugin.so ../DA-CREARE-Release/1-Linux/
cp -f ../libScreenSyncPlugin.so "$HOME/.config/OpenRGB/plugins/"
echo "Installato. Riavvia OpenRGB: comparira' la scheda 'Schermo'."
