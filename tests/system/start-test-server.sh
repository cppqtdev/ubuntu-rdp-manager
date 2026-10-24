#!/bin/bash
# (Re)creates the disposable xrdp test server container "urdm-srv" on network "urdm-test",
# installs a throwaway TLS certificate signed by a throwaway test CA, and optionally a staged
# Ubuntu RDP Manager installation.
#
# Usage: start-test-server.sh <pki-dir> [stage-dir]
#   pki-dir   : created by make-test-pki.sh (needs server.crt/server.key for host "urdm-srv")
#   stage-dir : DESTDIR of "cmake --install" (optional)
set -euo pipefail
PKI="${1:?pki dir}"
STAGE="${2:-}"
NAME="${URDM_SERVER:-urdm-srv}"
NET="${URDM_NET:-urdm-test}"

docker network inspect "$NET" >/dev/null 2>&1 || docker network create "$NET" >/dev/null
docker rm -f "$NAME" >/dev/null 2>&1 || true
# --cgroupns=private + SYS_ADMIN: systemd as PID 1 manages only the container's own cgroup subtree.
# SYS_PTRACE: lets root read /proc/<pid>/fd of the xrdp user's processes, as root can on a real host.
docker run -d --name "$NAME" --hostname "$NAME" --network "$NET" \
    --cgroupns=private --tmpfs /run --tmpfs /run/lock --tmpfs /tmp:exec \
    --cap-add SYS_ADMIN --cap-add SYS_PTRACE --security-opt apparmor=unconfined \
    urdm-xrdp-server:24.04 >/dev/null

for _ in $(seq 1 30); do
    state=$(docker exec "$NAME" systemctl is-system-running 2>/dev/null || true)
    [[ "$state" == running || "$state" == degraded ]] && break
    sleep 1
done
echo "systemd: $state"

docker cp "$PKI/server.crt" "$NAME:/etc/xrdp/urdm-test-cert.pem"
docker cp "$PKI/server.key" "$NAME:/etc/xrdp/urdm-test-key.pem"
docker exec "$NAME" bash -c '
    cp /etc/xrdp/sesman.ini /root/sesman.ini.orig; cp /etc/xrdp/xrdp.ini /root/xrdp.ini.orig
    chown root:root /etc/xrdp/urdm-test-cert.pem && chmod 0644 /etc/xrdp/urdm-test-cert.pem
    chown root:xrdp /etc/xrdp/urdm-test-key.pem && chmod 0640 /etc/xrdp/urdm-test-key.pem
    sed -i "s|^certificate=.*|certificate=/etc/xrdp/urdm-test-cert.pem|; s|^key_file=.*|key_file=/etc/xrdp/urdm-test-key.pem|" /etc/xrdp/xrdp.ini
    systemctl restart xrdp'

if [[ -n "$STAGE" ]]; then
    docker cp "$STAGE/." "$NAME:/"
    # docker cp keeps the host uid; a .deb installs root-owned files
    docker exec "$NAME" bash -c 'chown -R root:root /opt/ubuntu-rdp-manager /usr/share/dbus-1/system-services/com.urdm.Helper1.service /usr/share/dbus-1/system.d/com.urdm.Helper1.conf /usr/lib/systemd/system/urdm-helper.service /usr/share/polkit-1/actions/com.urdm.policy && systemctl daemon-reload && systemctl reload dbus'
    echo "staged installation copied"
fi
docker exec "$NAME" systemctl is-active xrdp xrdp-sesman
