#!/bin/zsh
set -e

ROOT="${0:A:h}"
HFE="${1:-/Users/dbk/Documents/EPS Files/_____EPS 16 filesOLD/ED-001_GKH.hfe}"
ROM="$ROOT/work/generated/eps16plus-rom.bin"
KPC_ROM="/Users/dbk/Downloads/Ensoniq EPS KPC2 v2.33 27c256.BIN"
OS_DISK="$ROOT/work/generated/EPS130OS.img"
PROBE="$ROOT/work/build/eps16_rom_probe"
PORT=8161

for FILE in "$HFE" "$ROM" "$KPC_ROM" "$OS_DISK" "$PROBE"; do
  if [[ ! -f "$FILE" ]]; then
    echo "Fehlt: $FILE"
    read '?Enter zum Schließen '
    exit 1
  fi
done

if lsof -nP -iTCP:$PORT -sTCP:LISTEN >/dev/null 2>&1; then
  echo "Test-Port $PORT ist bereits belegt. Die laufende Instanz bleibt unangetastet."
  open -a "Google Chrome" "http://127.0.0.1:$PORT"
  exit 0
fi

( sleep 1; open -a "Google Chrome" "http://127.0.0.1:$PORT" ) &
echo "Original-KPC-Test: http://127.0.0.1:$PORT"
echo "Audioausgabe ist für diesen Paneltest deaktiviert."

EPS16_LIVE=1 \
EPS16_LIVE_NO_AUDIO=1 \
EPS16_HTTP_PORT=$PORT \
EPS16_KPC_ROM="$KPC_ROM" \
EPS16_KPC_EXECUTE=1 \
EPS16_PANEL_DIR="$ROOT/panel" \
EPS16_OS_DISK="$OS_DISK" \
EPS16_SWAP_DISK="$HFE" \
  "$PROBE" "$ROM" 2147483647 "$OS_DISK"
