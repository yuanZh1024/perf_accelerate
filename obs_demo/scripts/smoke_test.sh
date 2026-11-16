#!/bin/sh
# smoke_test.sh - end to end check of obsd over HTTP.
#
# Starts the daemon on a temporary data directory, writes objects through
# the HTTP API, verifies byte for byte round trips, then damages shards on
# disk to prove the 4+2 erasure coding does its job.
#
#   ./scripts/smoke_test.sh [path-to-obsd]

set -u

OBSD=${1:-./obsd}
PORT=${PORT:-19000}
BASE="http://127.0.0.1:$PORT"
WORK=$(mktemp -d 2>/dev/null || echo /tmp/obsd-smoke-$$)
DATA="$WORK/data"
mkdir -p "$DATA" "$WORK/out"

fails=0
ok()   { printf '  \033[32mok\033[0m   %s\n' "$*"; }
bad()  { printf '  \033[31mFAIL\033[0m %s\n' "$*"; fails=$((fails + 1)); }
info() { printf '\n== %s ==\n' "$*"; }

cleanup() {
    if [ -n "${PID:-}" ]; then
        kill "$PID" 2>/dev/null
        sleep 0.2
    fi
    rm -rf "$WORK"
}
trap cleanup EXIT INT TERM

code() { curl -s -o /dev/null -w '%{http_code}' "$@"; }

info "starting obsd (data dir: $DATA)"
"$OBSD" -d "$DATA" -l 127.0.0.1 -p "$PORT" -k 4 -m 2 -s 64K >"$WORK/obsd.log" 2>&1 &
PID=$!

i=0
while [ $i -lt 50 ]; do
    if [ "$(code --max-time 1 "$BASE/v1/health")" = "200" ]; then break; fi
    i=$((i + 1))
    sleep 0.1
done
[ "$(code "$BASE/v1/health")" = "200" ] && ok "daemon is up on port $PORT" || { bad "daemon did not start"; cat "$WORK/obsd.log"; exit 1; }

# ---------------------------------------------------------------- basic io
info "write / read round trip"
head -c 5000000 /dev/urandom > "$WORK/big.bin"

out=$(curl -s -X PUT --data-binary @"$WORK/big.bin" "$BASE/v1/objects/big.bin")
echo "$out" | grep -q '"etag"' && ok "PUT 5000000 bytes -> $out" || bad "PUT failed: $out"

curl -s -o "$WORK/out/big.bin" "$BASE/v1/objects/big.bin"
if cmp -s "$WORK/big.bin" "$WORK/out/big.bin"; then
    ok "GET returns identical bytes"
else
    bad "GET content differs"
fi

# a few edge case sizes
for n in 0 1 262143 262144 262145 1048576; do
    head -c "$n" /dev/urandom > "$WORK/edge.bin" 2>/dev/null || : > "$WORK/edge.bin"
    curl -s -X PUT --data-binary @"$WORK/edge.bin" "$BASE/v1/objects/edge-$n.bin" >/dev/null
    curl -s -o "$WORK/out/edge.bin" "$BASE/v1/objects/edge-$n.bin"
    if cmp -s "$WORK/edge.bin" "$WORK/out/edge.bin"; then
        ok "size $n round trip"
    else
        bad "size $n round trip"
    fi
done

info "metadata endpoints"
hdr=$(curl -s -I "$BASE/v1/objects/big.bin")
echo "$hdr" | grep -qi 'X-EC-Scheme: 4+2' && ok "HEAD reports X-EC-Scheme: 4+2" || bad "HEAD missing EC header: $hdr"
echo "$hdr" | grep -qi 'ETag:' && ok "HEAD reports an ETag" || bad "HEAD missing ETag"

list=$(curl -s "$BASE/v1/objects?prefix=edge-")
echo "$list" | grep -q '"count":6' && ok "list with prefix returns 6 objects" || bad "list: $list"

stats=$(curl -s "$BASE/v1/stats")
echo "$stats" | grep -q '"ec":"4+2"' && ok "stats: $stats" || bad "stats: $stats"

# ---------------------------------------------------------------- layout
info "on disk layout"
n=$(find "$DATA/data" -name 'shard_*.ec' | wc -l | tr -d ' ')
[ "$n" -ge 36 ] && ok "$n shard files on disk (6 per object)" || bad "expected >= 36 shard files, found $n"
echo "  sample:"
find "$DATA/data" -name 'shard_0.ec' | head -1 | while read -r f; do
    printf '    %s\n' "${f#$DATA/}"
    ls -l "$f" | awk '{printf "    size: %s bytes\n", $5}'
done

# ---------------------------------------------------------------- failures
info "simulate disk / node failures"
kill_shards() { # $1=key $2..=shard indices to remove
    key=$1; shift
    for i in "$@"; do rm -f "$DATA/data/d$i/$key/shard_$i.ec"; done
}

kill_shards big.bin 0 2
[ "$(code "$BASE/v1/objects/big.bin")" = "200" ] && ok "GET works with shards d0 and d2 deleted" \
    || bad "GET failed after losing 2 shards"
curl -s -o "$WORK/out/big.bin" "$BASE/v1/objects/big.bin"
cmp -s "$WORK/big.bin" "$WORK/out/big.bin" && ok "rebuilt data is byte identical" || bad "rebuilt data differs"

# silent corruption: one shard missing plus one shard with a flipped byte
# is still within the 4+2 budget, and the CRC must catch the damaged shard
curl -s -X PUT --data-binary @"$WORK/big.bin" "$BASE/v1/objects/corrupt.bin" >/dev/null
kill_shards corrupt.bin 0
f="$DATA/data/d5/corrupt.bin/shard_5.ec"
if [ -f "$f" ]; then
    printf '\377\377\377' | dd of="$f" bs=1 seek=4096 conv=notrunc 2>/dev/null
    [ "$(code "$BASE/v1/objects/corrupt.bin")" = "200" ] && ok "GET works with d0 missing and d5 corrupted" \
        || bad "GET failed with a corrupted parity shard"
    curl -s -o "$WORK/out/corrupt.bin" "$BASE/v1/objects/corrupt.bin"
    cmp -s "$WORK/big.bin" "$WORK/out/corrupt.bin" && ok "rebuilt data is byte identical (CRC caught it)" \
        || bad "data differs after corruption"
else
    bad "shard d5 not found"
fi

# one failure too many: 3 of 6 shards left, must fail cleanly
kill_shards big.bin 1
c=$(code "$BASE/v1/objects/big.bin")
[ "$c" = "503" ] && ok "GET with only 3/6 shards returns 503 (no silent corruption)" \
    || bad "expected 503 with 3/6 shards, got $c"

info "delete"
c=$(code -X DELETE "$BASE/v1/objects/edge-0.bin")
[ "$c" = "204" ] && ok "DELETE returns 204" || bad "DELETE returned $c"
c=$(code "$BASE/v1/objects/edge-0.bin")
[ "$c" = "404" ] && ok "GET after DELETE returns 404" || bad "GET after DELETE returned $c"

info "result"
if [ "$fails" -eq 0 ]; then
    printf '  \033[32mALL CHECKS PASSED\033[0m\n'
    exit 0
fi
printf '  \033[31m%d CHECK(S) FAILED\033[0m\n' "$fails"
printf '  daemon log:\n'
sed 's/^/    /' "$WORK/obsd.log" | tail -30
exit 1
