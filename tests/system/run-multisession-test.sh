#!/bin/bash
# Multi-user concurrent RDP acceptance test (Tests C/D/I in docs/testing.md).
#
# Starts N independent FreeRDP 3 client containers, each logging in as a different
# Linux account (rdpuser1..rdpuserN) against the disposable xrdp server container,
# and records per-user login time, server-side session evidence, resource usage and
# a screenshot of every remote desktop.
#
# Usage: run-multisession-test.sh <N> <results-dir>
# Optional: URDM_CLIENT_ARGS (e.g. /cert:fingerprint:sha256:<fp> to pin a self-signed certificate)
# Requires: running server container "urdm-srv" on docker network "urdm-test",
#           image urdm-rdp-client:24.04, a test CA at $URDM_PKI/ca.crt.
set -euo pipefail

N="${1:?number of users}"
OUT="${2:?results dir}"
SERVER="${URDM_SERVER:-urdm-srv}"
NET="${URDM_NET:-urdm-test}"
HERE="$(cd "$(dirname "$0")" && pwd)"
PKI="${URDM_PKI:?set URDM_PKI to the directory created by make-test-pki.sh}"
LOGIN_TIMEOUT="${URDM_LOGIN_TIMEOUT:-90}"

mkdir -p "$OUT"
exec > >(tee "$OUT/run.log") 2>&1

srv() { docker exec "$SERVER" "$@"; }
now_ms() { date +%s%3N; }

echo "== Multi-session test: N=$N server=$SERVER $(date -Is)"
echo "== xrdp version: $(srv dpkg-query -W -f='${Version}' xrdp)"
echo "== sesman [Sessions]:"; srv sed -n '/^\[Sessions\]/,/^\[/p' /etc/xrdp/sesman.ini | grep -v '^[;#]' | grep -v '^$'
echo "== baseline memory:"; srv free -m

srv bash -c 'cat /proc/loadavg' > "$OUT/loadavg-before.txt"
srv free -m > "$OUT/free-before.txt"

declare -A START END
for i in $(seq 1 "$N"); do
    c="urdm-cli$i"
    docker rm -f "$c" >/dev/null 2>&1 || true
    docker run -d --name "$c" --network "$NET" --env-file "$HERE/test-credentials.env" \
        urdm-rdp-client:24.04 >/dev/null
    docker cp "$PKI/ca.crt" "$c:/usr/local/share/ca-certificates/urdm-test-ca.crt"
    docker exec "$c" update-ca-certificates >/dev/null 2>&1
done

# Launch all clients nearly simultaneously.
for i in $(seq 1 "$N"); do
    START[$i]=$(now_ms)
    docker exec -d "urdm-cli$i" sh -c "rdp-connect.sh $SERVER rdpuser$i 1 ${URDM_CLIENT_ARGS:-} > /tmp/rdp.log 2>&1"
done

# A login counts as complete when the user's desktop session process exists on the server.
pending=$N
deadline=$(( $(date +%s) + LOGIN_TIMEOUT ))
while (( pending > 0 && $(date +%s) < deadline )); do
    pending=0
    for i in $(seq 1 "$N"); do
        [[ -n "${END[$i]:-}" ]] && continue
        if srv pgrep -u "rdpuser$i" -x xfce4-session >/dev/null 2>&1; then
            END[$i]=$(now_ms)
        else
            pending=$((pending + 1))
        fi
    done
    sleep 0.5
done

sleep 15   # let desktops finish painting before screenshots and measurements

pass=0; fail=0
printf "%-10s %-8s %-12s %-10s %s\n" USER RESULT LOGIN_MS DISPLAY XFREERDP > "$OUT/summary.txt"
for i in $(seq 1 "$N"); do
    u="rdpuser$i"
    disp=$(srv bash -c "pgrep -u $u -a Xorg | grep -o ' :[0-9]*' | head -1" || true)
    client_alive=$(docker exec "urdm-cli$i" pgrep -x xfreerdp3 >/dev/null && echo alive || echo exited)
    docker exec "urdm-cli$i" sh -c 'DISPLAY=:1 import -window root /tmp/desktop.png' 2>/dev/null \
        && docker cp "urdm-cli$i:/tmp/desktop.png" "$OUT/desktop-$u.png" >/dev/null || true
    docker cp "urdm-cli$i:/tmp/rdp.log" "$OUT/client-$u.log" >/dev/null 2>&1 || true
    if [[ -n "${END[$i]:-}" && "$client_alive" == alive ]]; then
        result=PASS; pass=$((pass + 1)); ms=$(( END[$i] - START[$i] ))
    else
        result=FAIL; fail=$((fail + 1)); ms=-
    fi
    printf "%-10s %-8s %-12s %-10s %s\n" "$u" "$result" "$ms" "${disp:--}" "$client_alive" >> "$OUT/summary.txt"
done

srv loginctl list-sessions --no-legend > "$OUT/loginctl-sessions.txt" || true
srv bash -c 'for s in $(loginctl list-sessions --no-legend | awk "{print \$1}"); do
    loginctl show-session "$s" -p Id -p Name -p Service -p Type -p Class -p State -p Remote -p Display -p Leader; echo; done' \
    > "$OUT/loginctl-show.txt" || true
srv ps -eo user,pid,rss,pcpu,comm --sort=user > "$OUT/processes.txt"
srv free -m > "$OUT/free-after.txt"
srv cat /proc/loadavg > "$OUT/loadavg-after.txt"
# Per-user resident memory (sum of RSS across the user's processes), MiB.
srv bash -c "for i in \$(seq 1 $N); do u=rdpuser\$i; ps -u \$u -o rss= | awk -v u=\$u '{s+=\$1} END {printf \"%s %.1f\n\", u, s/1024}'; done" \
    > "$OUT/per-user-rss-mib.txt"
docker stats --no-stream --format '{{.Name}} {{.CPUPerc}} {{.MemUsage}}' "$SERVER" > "$OUT/container-stats.txt"
srv journalctl -u xrdp -u xrdp-sesman --no-pager -o short-iso --since "-10min" > "$OUT/journal.txt" || true

echo "== Summary"; cat "$OUT/summary.txt"
echo "== Per-user RSS (MiB)"; cat "$OUT/per-user-rss-mib.txt"
echo "== Memory after"; cat "$OUT/free-after.txt"
echo "== Load after: $(cat "$OUT/loadavg-after.txt")"
echo "== Container stats: $(cat "$OUT/container-stats.txt")"
echo "== RESULT: $pass passed, $fail failed of $N"
[[ $fail -eq 0 ]]
