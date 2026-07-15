#!/bin/zsh
set -e

ROOT="${0:A:h}"
HFE="${1:-/Users/dbk/Documents/EPS Files/_____EPS 16 filesOLD/ED-001_GKH.hfe}"
ROM="$ROOT/work/generated/eps16plus-rom.bin"
KPC_ROM="/Users/dbk/Downloads/Ensoniq EPS KPC2 v2.33 27c256.BIN"
OS_DISK="$ROOT/work/generated/EPS130OS.img"
PROBE="$ROOT/work/build/eps16_rom_probe"
SAMPLE_TRACE="/tmp/eps16-sampling-live.log"

for FILE in "$HFE" "$ROM" "$KPC_ROM" "$OS_DISK" "$PROBE"; do
  if [[ ! -f "$FILE" ]]; then
    echo "Fehlt: $FILE"
    read '?Enter zum Schließen '
    exit 1
  fi
done

# A double-click is also the explicit restart operation.  Replace only an
# already running probe whose full command line contains this project's exact
# executable path; never kill an unrelated service merely because it owns the
# same port.
OLD_PIDS=( ${(f)"$(lsof -tiTCP:8160 -sTCP:LISTEN 2>/dev/null || true)"} )
for OLD_PID in $OLD_PIDS; do
  [[ -n "$OLD_PID" && "$OLD_PID" != "$$" ]] || continue
  OLD_COMMAND="$(ps -p "$OLD_PID" -o command= 2>/dev/null || true)"
  [[ "$OLD_COMMAND" == *"$PROBE"* ]] || continue
  echo "Beende vorherige EPS-16-Instanz ($OLD_PID) …"
  kill "$OLD_PID" 2>/dev/null || true
done
for ATTEMPT in {1..30}; do
  lsof -nP -iTCP:8160 -sTCP:LISTEN >/dev/null 2>&1 || break
  sleep 0.1
done
if lsof -nP -iTCP:8160 -sTCP:LISTEN >/dev/null 2>&1; then
  echo "Port 8160 ist noch belegt. Bitte das alte Emulator-Terminal schließen."
  read '?Enter zum Schließen '
  exit 1
fi

( sleep 1; open http://127.0.0.1:8160 ) &
echo "Sampling-Diagnose: $SAMPLE_TRACE"
EPS16_LIVE=1 \
EPS16_TRACE_SAMPLE_LIVE="$SAMPLE_TRACE" \
EPS16_KPC_ROM="$KPC_ROM" \
EPS16_KPC_EXECUTE=1 \
EPS16_PANEL_DIR="$ROOT/panel" \
EPS16_OS_DISK="$OS_DISK" \
EPS16_SWAP_DISK="$HFE" \
  "$PROBE" "$ROM" 2147483647 "$OS_DISK"
