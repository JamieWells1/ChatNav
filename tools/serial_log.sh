#!/usr/bin/env bash
# Shows the Pico's USB serial log with a wall-clock time on every line and saves it to a file.
#
#   tools/serial_log.sh [/dev/cu.usbmodemXXXX] [out.log]
#
# With no port it picks the first /dev/cu.usbmodem*. Close any other serial program first
# (screen, Arduino, wav_capture): only one program can hold the port. Ctrl-C stops it.
set -euo pipefail

port="${1:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -1 || true)}"
out="${2:-serial_$(date +%Y%m%d_%H%M%S).log}"
[ -n "$port" ] && [ -e "$port" ] || { echo "no Pico serial port found (is it plugged in and not in BOOTSEL?)" >&2; exit 1; }

echo "[LOG] reading $port, saving to $out (Ctrl-C to stop)" >&2
stty -f "$port" 115200 raw -echo
cat "$port" | python3 -u -c '
import sys, time
for line in sys.stdin.buffer:
    t = time.time()
    stamp = time.strftime("%H:%M:%S", time.localtime(t)) + ".%03d" % int((t % 1) * 1000)
    sys.stdout.write(stamp + " " + line.decode(errors="replace"))
    sys.stdout.flush()
' | tee "$out"
