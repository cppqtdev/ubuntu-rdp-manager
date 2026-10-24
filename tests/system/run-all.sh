#!/bin/bash
# Runs every container-based system test against a build tree, in disposable Docker containers.
# Nothing is installed on the host.
#
# Usage: tests/system/run-all.sh <cmake-build-dir> <results-dir> [--with-load]
#   --with-load  also run the 2/5/10 concurrent-user load tests (several GiB of RAM)
set -uo pipefail
BUILD="${1:?build dir}"; OUT="${2:?results dir}"; LOAD="${3:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$OUT"
status=0

echo "### images"
docker build -q -t urdm-xrdp-server:24.04 "$HERE/docker/server" >/dev/null
docker build -q -t urdm-rdp-client:24.04 "$HERE/docker/client" >/dev/null

echo "### stage install of $BUILD"
DESTDIR="$WORK/stage" cmake --install "$BUILD" >/dev/null
"$HERE/make-test-pki.sh" "$WORK/pki" urdm-srv >/dev/null
export URDM_PKI="$WORK/pki"

echo "### helper integration"
for i in $(seq 1 10); do docker rm -f "urdm-cli$i" >/dev/null 2>&1; done
"$HERE/start-test-server.sh" "$WORK/pki" "$WORK/stage" >/dev/null
docker exec urdm-srv bash -c 'rm -f /home/rdpuser*/.xsession'   # use the packaged desktop script
"$ROOT/tests/integration/helper-integration.sh" "$OUT/helper-integration" | grep -E "FAIL|RESULT" || status=1

echo "### GUI acceptance"
for i in $(seq 1 10); do docker rm -f "urdm-cli$i" >/dev/null 2>&1; done
"$HERE/start-test-server.sh" "$WORK/pki" "$WORK/stage" >/dev/null
DEPS=$(tr '\n' ' ' < "$ROOT/packaging/debian/runtime-depends.txt")
docker exec urdm-srv bash -c "rm -f /home/rdpuser*/.xsession; apt-get update -qq >/dev/null 2>&1; DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends xdotool imagemagick xvfb xauth $DEPS >/dev/null 2>&1"
"$HERE/gui-in-container.sh" "$OUT/gui-pages" light 1366x768 >/dev/null 2>&1      # warms caches, captures every page
"$HERE/gui-in-container.sh" "$OUT/gui-pages" dark 1366x768 0 3 5 9 >/dev/null 2>&1
for i in 1 2 3; do
    docker run -d --name "urdm-cli$i" --network urdm-test --env-file "$HERE/test-credentials.env" urdm-rdp-client:24.04 >/dev/null
    docker cp "$WORK/pki/ca.crt" "urdm-cli$i:/usr/local/share/ca-certificates/t.crt"
    docker exec "urdm-cli$i" update-ca-certificates >/dev/null 2>&1
    docker exec -d "urdm-cli$i" sh -c "rdp-connect.sh urdm-srv rdpuser$i 1 > /tmp/rdp.log 2>&1"
done
sleep 15
docker exec urdm-cli3 pkill -x xfreerdp3   # one disconnected session
"$HERE/gui-acceptance.sh" "$OUT/gui-acceptance" | grep -E "FAIL|RESULT" || status=1

if [[ "$LOAD" == --with-load ]]; then
    for n in 2 5 10; do
        echo "### $n concurrent users"
        for i in $(seq 1 10); do docker rm -f "urdm-cli$i" >/dev/null 2>&1; done
        "$HERE/start-test-server.sh" "$WORK/pki" "$WORK/stage" >/dev/null
        docker exec urdm-srv bash -c 'sed -i "s/^MaxSessions=.*/MaxSessions=9999/" /etc/xrdp/sesman.ini; systemctl restart xrdp'
        "$HERE/run-multisession-test.sh" "$n" "$OUT/users-$n" | grep -E "RESULT" || status=1
    done
fi
for i in $(seq 1 10); do docker rm -f "urdm-cli$i" >/dev/null 2>&1; done
echo "### overall: $([[ $status -eq 0 ]] && echo PASS || echo FAIL)"
exit $status
