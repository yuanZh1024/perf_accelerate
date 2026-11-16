#!/bin/sh
# install.sh - install obs-ec-store on Ubuntu 22.04 (systemd).
#
#   sudo ./deploy/install.sh            # build, install, enable and start
#   sudo PORT=9100 ./deploy/install.sh  # different port
#
# Re-running the script upgrades the binary and restarts the service.
# Existing data under $DATA is never touched.

set -eu

PREFIX=${PREFIX:-/usr/local}
DATA=${DATA:-/var/lib/obsd}
PORT=${PORT:-9000}
SERVICE_USER=${SERVICE_USER:-obsd}
SERVICE_NAME=obsd

say() { printf '\033[1m==> %s\033[0m\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

[ "$(id -u)" -eq 0 ] || die "please run as root (sudo $0)"
[ -f Makefile ] || die "run this script from the project root"

# ---------------------------------------------------------------- build
say "building"
make clean >/dev/null 2>&1 || true
make -j"$(nproc)"

# ---------------------------------------------------------------- install
say "installing binary to $PREFIX/bin"
make install PREFIX="$PREFIX"

say "creating service user and data directory"
if ! id -u "$SERVICE_USER" >/dev/null 2>&1; then
    useradd --system --home-dir "$DATA" --shell /usr/sbin/nologin "$SERVICE_USER"
fi
mkdir -p "$DATA"
chown -R "$SERVICE_USER:$SERVICE_USER" "$DATA"
chmod 750 "$DATA"

# ---------------------------------------------------------------- unit
say "installing systemd unit"
sed -e "s|-d /var/lib/obsd|-d $DATA|" \
    -e "s|-p 9000|-p $PORT|" \
    -e "s|/usr/local/bin/obsd|$PREFIX/bin/obsd|" \
    deploy/obsd.service > "/etc/systemd/system/$SERVICE_NAME.service"

systemctl daemon-reload
systemctl enable "$SERVICE_NAME" >/dev/null
systemctl restart "$SERVICE_NAME"

# ---------------------------------------------------------------- check
say "waiting for the daemon"
i=0
while [ $i -lt 50 ]; do
    if curl -fsS "http://127.0.0.1:$PORT/v1/health" >/dev/null 2>&1; then break; fi
    i=$((i + 1))
    sleep 0.2
done

if curl -fsS "http://127.0.0.1:$PORT/v1/health" >/dev/null 2>&1; then
    say "up and running"
    curl -s "http://127.0.0.1:$PORT/v1/stats"
    printf '\n'
    printf '  data directory : %s\n' "$DATA"
    printf '  endpoint       : http://0.0.0.0:%s\n' "$PORT"
    printf '  logs           : journalctl -u %s -f\n' "$SERVICE_NAME"
else
    printf 'error: the daemon did not answer on port %s\n' "$PORT" >&2
    journalctl -u "$SERVICE_NAME" -n 30 --no-pager >&2 || true
    exit 1
fi
