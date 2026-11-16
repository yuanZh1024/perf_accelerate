#!/bin/sh
# demo.sh - walk through what the erasure coding actually does on disk.
#
#   ./scripts/demo.sh [path-to-obsd]
#
# Starts the daemon on a scratch directory, stores a 4 MiB object, shows the
# six shard files it produced, then removes two of them and reads the object
# back to prove nothing was lost.

set -u

OBSD=${1:-./obsd}
PORT=${PORT:-9000}
BASE="http://127.0.0.1:$PORT"
WORK=$(mktemp -d 2>/dev/null || echo /tmp/obsd-demo-$$)
DATA="$WORK/data"
KEY=demo/photo.raw

mkdir -p "$DATA"

cleanup() {
    if [ -n "${PID:-}" ]; then kill "$PID" 2>/dev/null; sleep 0.2; fi
    rm -rf "$WORK"
}
trap cleanup EXIT INT TERM

hr() { printf '\n\033[1m%s\033[0m\n' "---- $* ----"; }

[ -x "$OBSD" ] || { echo "building..."; make -s; OBSD=./obsd; }

hr "start the daemon (4+2, 256K shards => 1 MiB stripes)"
"$OBSD" -d "$DATA" -l 127.0.0.1 -p "$PORT" -k 4 -m 2 -s 256K >"$WORK/obsd.log" 2>&1 &
PID=$!
i=0
while [ $i -lt 50 ]; do
    curl -fsS "$BASE/v1/health" >/dev/null 2>&1 && break
    i=$((i + 1)); sleep 0.1
done

hr "write a 4 MiB object"
head -c 4194304 /dev/urandom > "$WORK/photo.raw"
printf 'source file: %s bytes\n' "$(wc -c < "$WORK/photo.raw" | tr -d ' ')"
curl -s -X PUT --data-binary @"$WORK/photo.raw" "$BASE/v1/objects/$KEY"
printf '\n'

hr "what landed on disk"
find "$DATA/data" -name 'shard_*.ec' | sort | while read -r f; do
    printf '  %-46s %8s bytes\n' "${f#$DATA/}" "$(wc -c < "$f" | tr -d ' ')"
done
printf '\n  note: 4 data shards + 2 parity shards, each ~1 MiB = 1/4 of the object\n'
printf '        total on disk is 1.5x the object size (4+2 = 50%% overhead)\n'

hr "read it back"
curl -s -o "$WORK/back.raw" "$BASE/v1/objects/$KEY"
cmp -s "$WORK/photo.raw" "$WORK/back.raw" && echo "  byte for byte identical" || echo "  MISMATCH"

hr "delete shards d0 and d1 (simulate two disks dying)"
rm -f "$DATA/data/d0/$KEY/shard_0.ec" "$DATA/data/d1/$KEY/shard_1.ec"
find "$DATA/data" -name 'shard_*.ec' | sort | while read -r f; do
    printf '  %-46s %8s bytes\n' "${f#$DATA/}" "$(wc -c < "$f" | tr -d ' ')"
done

hr "read it again - the daemon rebuilds the missing shards on the fly"
curl -s -o "$WORK/back2.raw" "$BASE/v1/objects/$KEY"
cmp -s "$WORK/photo.raw" "$WORK/back2.raw" && echo "  byte for byte identical" || echo "  MISMATCH"
printf '\n  daemon log:\n'
grep -E 'rebuild|healthy' "$WORK/obsd.log" | tail -6 | sed 's/^/    /'

hr "cluster view"
curl -s "$BASE/v1/stats"
printf '\n'

hr "done"
echo "  the scratch data directory is removed on exit"
