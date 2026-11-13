#!/bin/bash
# 컴파일만:        ./tools/build.sh
# 컴파일 + 굽기:   ./tools/build.sh /dev/cu.usbmodem1101
set -e
cd "$(dirname "$0")/../firmware/voicenote"

FQBN="esp32:esp32:esp32s3:PSRAM=opi,CDCOnBoot=cdc,USBMode=hwcdc"
BUILD="${TMPDIR:-/tmp}/voicenote_build"

arduino-cli compile --fqbn "$FQBN" --build-path "$BUILD" .

if [ -n "$1" ]; then
  arduino-cli upload --fqbn "$FQBN" --input-dir "$BUILD" -p "$1" .
  echo "--- 로그 (끝내려면 Ctrl+C) ---"
  arduino-cli monitor -p "$1" -c baudrate=115200
fi
