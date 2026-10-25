#!/bin/sh
# Run an ELF (default build/bench.elf) in WoWDC's cache/pipeline-timing Flycast and print the bench lines.
# Isolated config in a temp dir, removed on exit. FLYCAST= overrides the emulator.
set -eu
elf=${1:-build/bench.elf}
fc=${FLYCAST:-$HOME/.local/share/wowdc/emulators/aica-cache/flycast}
tmp=$(mktemp -d /tmp/sh4dow-fc.XXXXXX)
trap 'rm -rf "$tmp"' EXIT INT TERM
mkdir -p "$tmp/config/flycast" "$tmp/data/flycast"
XDG_CONFIG_HOME=$tmp/config XDG_DATA_HOME=$tmp/data SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
timeout "${SECONDS_MAX:-150}" "$fc" \
    -config 'config:UseReios=yes,config:Dynarec.Enabled=yes,config:Debug.SerialConsoleEnabled=yes,audio:backend=null,config:rend.ThreadedRendering=no' \
    -cachesim-timing-pipeline -cachesim-data -cachesim-report "$tmp/cache.json" -cachesim-symbols "$elf" \
    -cachesim-frames 0 -cachesim-skip 0 "$elf" > "$tmp/log" 2>&1 &
pid=$!
# The bench prints its last line then idles: stop as soon as it is there.
while kill -0 "$pid" 2>/dev/null; do
    if grep -aq '^packed_n' "$tmp/log" 2>/dev/null; then kill "$pid" 2>/dev/null || true; break; fi
    sleep 1
done
wait "$pid" 2>/dev/null || true
grep -a -E '^(SH4DOW|prepare|every|list|packed|ab )' "$tmp/log" || { tail -20 "$tmp/log"; exit 1; }
