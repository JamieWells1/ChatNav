#!/usr/bin/env bash
# Copies the five firmware images into firmware/prebuilt/ (gitignored) and prints their sizes
# and SHA-256, so the owner can flash from one folder and quote the hash in a bug report.
#
#   tools/collect_uf2.sh             copy what is already built in firmware/build/
#   tools/collect_uf2.sh --rebuild   run cmake --build first (build dir must be configured)
#
# Never flashes anything.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
fw="$here/../firmware"
out="$fw/prebuilt"

if [ "${1:-}" = "--rebuild" ]; then
    cmake --build "$fw/build" -j8
fi

mkdir -p "$out"
for t in diag_gps diag_encoder diag_mic diag_amp chatnav; do
    src="$fw/build/$t.uf2"
    if [ ! -f "$src" ]; then
        echo "MISSING $src (run with --rebuild, or see docs/TESTING.md section 'Rebuild')" >&2
        exit 1
    fi
    cp "$src" "$out/$t.uf2"
    printf '%-18s %8s bytes  sha256 %s\n' "$t.uf2" "$(wc -c < "$out/$t.uf2" | tr -d ' ')" \
        "$(shasum -a 256 "$out/$t.uf2" | cut -d' ' -f1)"
done
echo "Images are in $out"
