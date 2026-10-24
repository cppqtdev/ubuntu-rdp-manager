#!/bin/bash
# UI-driven acceptance test: the real GUI runs inside the disposable xrdp server (Xvfb, light
# theme, 1366x768, no window manager so the window sits at 0,0) and is operated with xdotool.
# Every effect is verified independently through systemd, the kernel socket table, sesman.ini,
# logind and real FreeRDP 3 clients — never through the GUI's own state.
#
# Usage: URDM_PKI=<pki> gui-acceptance.sh <results-dir>
set -uo pipefail
OUT="${1:?results dir}"
PKI="${URDM_PKI:?}"
SRV=urdm-srv
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"
exec > >(tee "$OUT/gui-acceptance.log") 2>&1
pass=0; fail=0; n=0
ok()  { echo "PASS  $*"; pass=$((pass+1)); }
bad() { echo "FAIL  $*"; fail=$((fail+1)); }
srv() { docker exec "$SRV" "$@"; }
x()   { docker exec "$SRV" su urdmadmin -c "DISPLAY=:99 $*"; }
shot() { n=$((n+1)); x "import -window root /var/tmp/shot.png"; srv cat /var/tmp/shot.png > "$OUT/$(printf %02d $n)-$1.png"; echo "      screenshot $(printf %02d $n)-$1.png"; }
click() { x "xdotool mousemove $1 $2 click 1"; sleep "${3:-1.5}"; }
# Click the accent-coloured (highlighted) button inside a region of the current screen.
click_accent() {
    x "import -window root /var/tmp/find.png"; srv cat /var/tmp/find.png > "$OUT/.find.png"
    local xy; xy=$(python3 "$HERE/find-accent.py" "$OUT/.find.png" "$@") || { bad "accent button not found in region $*"; return 1; }
    click $xy 2
}
listening() { srv bash -c 'ss -ltn | grep -q ":3389 "'; }
connect_user() { # connect_user <client#> <user>
    local c="urdm-cli$1"
    docker rm -f "$c" >/dev/null 2>&1
    docker run -d --name "$c" --network urdm-test --env-file "$HERE/test-credentials.env" urdm-rdp-client:24.04 >/dev/null
    docker cp "$PKI/ca.crt" "$c:/usr/local/share/ca-certificates/t.crt"; docker exec "$c" update-ca-certificates >/dev/null 2>&1
    docker exec -d "$c" sh -c "rdp-connect.sh $SRV $2 1 > /tmp/rdp.log 2>&1"
}
# y coordinate of a user's row on the Sessions page (rows are sorted by display number, 52 px apart,
# first row at y=357 when no banner is shown)
row_y() {
    local idx
    idx=$(srv bash -c 'for s in $(loginctl list-sessions --no-legend | awk "{print \$1}"); do
            [ "$(loginctl show-session $s -p Service --value)" = xrdp-sesman ] || continue
            echo "$(loginctl show-session $s -p Display --value | tr -d :) $(loginctl show-session $s -p Name --value)"; done' \
          | sort -n | awk -v u="$1" '$2==u {print NR-1; exit}')
    echo $((357 + idx * 52))
}
wait_desktop() { for _ in $(seq 1 60); do srv pgrep -u "$1" -x xfce4-session >/dev/null && return 0; sleep 0.5; done; return 1; }

echo "=== GUI acceptance $(date -Is)"
srv pkill -f ubuntu-rdp-manager; srv pkill -f "Xvfb :99"; sleep 1
docker exec -d "$SRV" su urdmadmin -c 'Xvfb :99 -screen 0 1366x768x24 >/dev/null 2>&1 & sleep 1; cd ~; DISPLAY=:99 /opt/ubuntu-rdp-manager/bin/ubuntu-rdp-manager --page 2 --theme light --size 1366x768 >/var/tmp/gui.log 2>&1'
sleep 8
shot server-control-initial

echo "--- Test B: disable / enable RDP through the UI"
click 455 168                     # Turn off
shot stop-confirmation
click_accent 430 250 940 560      # "Stop server"
sleep 3
[[ "$(srv systemctl is-active xrdp)" == inactive ]] && ok "xrdp.service inactive after UI stop" || bad "xrdp still $(srv systemctl is-active xrdp)"
listening && bad "port 3389 still listening" || ok "port 3389 closed"
docker run --rm --network urdm-test urdm-rdp-client:24.04 sh -c 'nc -z -w 3 urdm-srv 3389' && bad "TCP connect succeeded while stopped" || ok "remote client cannot connect while stopped"
shot stopped
click 346 168 4                   # Turn on
[[ "$(srv systemctl is-active xrdp)" == active ]] && ok "xrdp.service active after UI start" || bad "xrdp $(srv systemctl is-active xrdp)"
listening && ok "port 3389 listening again" || bad "port not listening"
connect_user 4 rdpuser4
wait_desktop rdpuser4 && ok "new RDP login (rdpuser4) reached a desktop after UI start" || bad "rdpuser4 got no desktop"
shot started

echo "--- Autostart switch"
click 953 168 3
[[ "$(srv systemctl is-enabled xrdp)" == disabled ]] && ok "autostart disabled through UI" || bad "is-enabled=$(srv systemctl is-enabled xrdp)"
click 953 168 3
[[ "$(srv systemctl is-enabled xrdp)" == enabled ]] && ok "autostart re-enabled through UI" || bad "is-enabled=$(srv systemctl is-enabled xrdp)"

echo "--- Test E: unlimited sessions through the UI"
click 96 270 3                    # sidebar: Session Limits
shot session-limits-before
click 320 155 1                   # Unlimited switch
click_accent 1100 30 1366 70      # Apply (top right)
sleep 2
v=$(srv sed -n '/^\[Sessions\]/,/^\[/p' /etc/xrdp/sesman.ini | grep '^MaxSessions=')
[[ "$v" == "MaxSessions=9999" ]] && ok "sesman.ini now $v (0.9.24-safe unlimited)" || bad "sesman.ini has '$v'"
n_backups=$(srv bash -c 'ls /var/lib/ubuntu-rdp-manager/backups | grep -c sesman.ini')
[[ "$n_backups" -ge 1 ]] && ok "backup created ($n_backups sesman.ini backups)" || bad "no backup"
shot session-limits-applied
click 96 70 3                     # Dashboard
shot dashboard-unlimited

echo "--- Sessions page: clean up orphans, disconnect, reconnect (Test F), terminate"
click 96 190 3                    # sidebar: Active Sessions
shot sessions-orphans-after-restart
before=$(srv bash -c "loginctl list-sessions --no-legend | wc -l")
click 1226 281 1                  # banner: "Terminate abandoned…"
shot cleanup-confirmation
click_accent 430 200 940 600
sleep 5
after=$(srv bash -c "loginctl list-sessions --no-legend | wc -l")
srv pgrep -u rdpuser1 -x Xorg >/dev/null || srv pgrep -u rdpuser2 -x Xorg >/dev/null || srv pgrep -u rdpuser3 -x Xorg >/dev/null \
    && bad "orphaned desktops still running" || ok "UI cleanup ended the 3 orphaned desktops ($before -> $after sessions)"
srv pgrep -u rdpuser4 -x xfce4-session >/dev/null && ok "connected rdpuser4 untouched by cleanup" || bad "rdpuser4 affected"
connect_user 5 rdpuser5
wait_desktop rdpuser5 && ok "bystander rdpuser5 logged in" || bad "rdpuser5 no desktop"
sleep 4
shot sessions-two-connected
d4=$(srv bash -c "pgrep -u rdpuser4 -a Xorg | grep -o ' :[0-9]*'")
y4=$(row_y rdpuser4); echo "      rdpuser4 row at y=$y4"
click 1257 "$y4" 1                # rdpuser4: disconnect
click_accent 430 200 940 600
sleep 3
srv pgrep -u rdpuser4 -x Xorg >/dev/null && ok "rdpuser4 desktop keeps running after UI disconnect" || bad "rdpuser4 desktop gone"
docker exec urdm-cli4 pgrep -x xfreerdp3 >/dev/null && bad "client 4 still connected" || ok "client 4 disconnected"
docker exec urdm-cli5 pgrep -x xfreerdp3 >/dev/null && ok "rdpuser5 unaffected by the disconnect" || bad "client 5 dropped"
sleep 3
shot sessions-after-disconnect
docker exec -d urdm-cli4 sh -c 'pkill Xvfb; sleep 1; rdp-connect.sh urdm-srv rdpuser4 1 > /tmp/re.log 2>&1'
sleep 10
d4b=$(srv bash -c "pgrep -u rdpuser4 -a Xorg | grep -o ' :[0-9]*'")
n4=$(srv bash -c "pgrep -u rdpuser4 -c Xorg")
[[ "$d4b" == "$d4" && "$n4" == 1 ]] && ok "Test F: reconnect returned to the same desktop ($d4, Policy=Default)" || bad "reconnect: before='$d4' after='$d4b' count=$n4"
shot sessions-after-reconnect
s4=$(srv bash -c "loginctl list-sessions --no-legend | awk '\$3==\"rdpuser4\"{print \$1}'")
y4=$(row_y rdpuser4)
click 1295 "$y4" 1                # rdpuser4: terminate
shot terminate-confirmation
click_accent 430 200 940 600
sleep 4
srv pgrep -u rdpuser4 -x Xorg >/dev/null && bad "rdpuser4 Xorg still running" || ok "rdpuser4 session terminated through UI"
srv loginctl show-session "$s4" >/dev/null 2>&1 && bad "logind session $s4 still exists" || ok "logind session $s4 removed"
docker exec urdm-cli5 pgrep -x xfreerdp3 >/dev/null && ok "rdpuser5 still connected after terminating rdpuser4" || bad "client 5 dropped"
shot sessions-after-terminate

echo "--- Restart orphans running desktops (documented behaviour) and the UI reports it"
click 96 150 3                    # Server Control
click 563 244 1                   # Restart (shifted down by the "restart required" notice)
shot restart-confirmation
click_accent 430 200 940 600
sleep 5
click 96 190 4                    # Active Sessions
shot sessions-orphaned
orph=$(srv bash -c "loginctl list-sessions --no-legend | wc -l")
echo "      logind sessions still present after restart: $orph"
[[ "$orph" -ge 1 ]] && ok "desktops survive restart (as orphans)" || bad "no sessions survived"

srv cat /var/tmp/gui.log > "$OUT/gui-stderr.log"
grep -c "qrc:/" "$OUT/gui-stderr.log" | xargs echo "QML warnings in GUI log:"
srv tail -n 30 /var/log/ubuntu-rdp-manager/audit.log > "$OUT/audit-tail.log"
srv pkill -f ubuntu-rdp-manager; srv pkill -f "Xvfb :99"
rm -f "$OUT/.find.png"
echo "=== RESULT: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
