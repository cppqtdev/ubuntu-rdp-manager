#!/bin/bash
# Runs the real GUI inside the disposable xrdp server container (as a non-root administrator,
# against live systemd/logind/xrdp/polkit) and captures a screenshot of every page.
#
# Usage: gui-in-container.sh <out-dir> [theme] [WxH] [pages...]
# Preconditions: start-test-server.sh <pki> <stage> has installed the staged build.
set -euo pipefail
OUT="${1:?out dir}"; THEME="${2:-light}"; SIZE="${3:-1366x768}"; shift 3 || true
PAGES=("$@"); [[ ${#PAGES[@]} -eq 0 ]] && PAGES=(0 1 2 3 4 5 6 7 8 9 10)
SRV="${URDM_SERVER:-urdm-srv}"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"

# Runtime dependencies exactly as declared in the .deb, plus Xvfb for headless screenshots.
DEPS=$(tr '\n' ' ' < "$HERE/../../packaging/debian/runtime-depends.txt")
docker exec "$SRV" bash -c "dpkg -s xvfb >/dev/null 2>&1 || (apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends xvfb xauth $DEPS) >/dev/null 2>&1"

# Administrator test account (sudo + adm like Ubuntu's first user) and the TEST-ONLY polkit rule
# standing in for the password dialog (no polkit agent exists in the container).
docker exec "$SRV" bash -c 'id urdmadmin >/dev/null 2>&1 || useradd -m -s /bin/bash -G sudo,adm urdmadmin
usermod -aG adm urdmadmin
cat > /etc/polkit-1/rules.d/49-urdm-test.rules <<EOF
// TEST CONTAINER ONLY
polkit.addRule(function(action, subject) {
    if (action.id.indexOf("com.urdm.") == 0 && subject.user == "urdmadmin") return polkit.Result.YES;
});
EOF'

for p in "${PAGES[@]}"; do
    docker exec "$SRV" su urdmadmin -c "cd ~ && xvfb-run -a -s '-screen 0 2560x1440x24' /opt/ubuntu-rdp-manager/bin/ubuntu-rdp-manager --theme $THEME --size $SIZE --page $p --delay 6000 --screenshot /tmp/page-$p.png" 2>&1 | grep -v "stale" || true
    docker exec "$SRV" cat "/tmp/page-$p.png" > "$OUT/page-$p-$THEME-$SIZE.png"
done
docker exec "$SRV" su urdmadmin -c "cd ~ && xvfb-run -a -s '-screen 0 1920x1080x24' /opt/ubuntu-rdp-manager/bin/ubuntu-rdp-manager --smoke-test" 2>&1 | grep -v stale | tail -3
ls "$OUT"
