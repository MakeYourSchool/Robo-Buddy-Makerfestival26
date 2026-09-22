#!/usr/bin/env sh
# Build and copy the firmware the browser installer serves.
# Run after changing anything under src/, then commit docs/firmware/.
set -eu
cd "$(dirname "$0")/.."
pio run
cp .pio/build/esp32-c6-devkitc-1/firmware.factory.bin docs/firmware/mf26-standalone.bin
echo "docs/firmware/mf26-standalone.bin aktualisiert ($(du -h docs/firmware/mf26-standalone.bin | cut -f1))"
