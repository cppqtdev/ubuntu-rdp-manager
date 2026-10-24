#!/bin/bash
# Integration test of urdm-helper against real systemd, logind, polkit, xrdp and FreeRDP 3
# inside the disposable test container. Run from the host:
#   URDM_PKI=<pki> tests/integration/helper-integration.sh <results-dir>
# Preconditions: tests/system/start-test-server.sh <pki> <stage> has been run.
set -uo pipefail
OUT="${1:?results dir}"
PKI="${URDM_PKI:?}"
SRV=urdm-srv
NET=urdm-test
HERE="$(cd "$(dirname "$0")" && pwd)"
CREDS="$HERE/../system/test-credentials.env"
mkdir -p "$OUT"
exec > >(tee "$OUT/integration.log") 2>&1

pass=0; fail=0
ok()   { echo "PASS  $*"; pass=$((pass+1)); }
bad()  { echo "FAIL  $*"; fail=$((fail+1)); }
srv()  { docker exec "$SRV" "$@"; }
# Calls the helper as the given user. Prints the reply or the error.
call() { local u="$1"; shift; docker exec "$SRV" su "$u" -c "busctl call com.urdm.Helper1 /com/urdm/Helper1 com.urdm.Helper1 $(printf '%q ' "$@")" 2>&1; }
expect_ok()   { local d="$1"; shift; local r; r=$(call "$@"); if [[ "$r" == "s "* || "$r" == "a{sv} "* ]]; then ok "$d -> $r"; else bad "$d -> $r"; fi; }
expect_fail() { local d="$1" pat="$2"; shift 2; local r; r=$(call "$@"); if [[ "$r" == "Call failed"*"$pat"* ]]; then ok "$d -> $r"; else bad "$d (expected failure ~ '$pat') -> $r"; fi; }
ini()  { srv sed -n "/^\[$2\]/,/^\[/p" "/etc/xrdp/$1" | grep -i "^$3=" | head -1; }

client() { # client <n> <user>
    local c="urdm-cli$1"
    docker rm -f "$c" >/dev/null 2>&1
    docker run -d --name "$c" --network "$NET" --env-file "$CREDS" urdm-rdp-client:24.04 >/dev/null
    docker cp "$PKI/ca.crt" "$c:/usr/local/share/ca-certificates/urdm-test-ca.crt"
    docker exec "$c" update-ca-certificates >/dev/null 2>&1
    docker exec -d "$c" sh -c "rdp-connect.sh $SRV $2 1 > /tmp/rdp.log 2>&1"
}
session_of() { srv bash -c "loginctl list-sessions --no-legend | awk '\$3==\"$1\" {print \$1}' | head -1"; }
wait_session() { for _ in $(seq 1 40); do s=$(session_of "$1"); [[ -n "$s" ]] && srv pgrep -u "$1" -x xfce4-session >/dev/null && { echo "$s"; return; }; sleep 0.5; done; }

echo "=== Integration run $(date -Is), xrdp $(srv dpkg-query -W -f='${Version}' xrdp)"

# Test administrator. There is no polkit agent in the container, so a TEST-ONLY rule stands in
# for the administrator typing their password. Production uses the shipped auth_admin_keep defaults.
srv bash -c 'id urdmadmin >/dev/null 2>&1 || useradd -m -s /bin/bash -G sudo urdmadmin
cat > /etc/polkit-1/rules.d/49-urdm-test.rules <<EOF
// TEST CONTAINER ONLY
polkit.addRule(function(action, subject) {
    if (action.id.indexOf("com.urdm.") == 0 && subject.user == "urdmadmin") return polkit.Result.YES;
});
EOF'
srv usermod -aG sudo rdpuser2   # an admin without the test rule: must still be refused non-interactively

echo "--- 1. Authorization"
expect_fail "non-admin cannot change config" "not authorized" rdpuser3 ApplyConfigChanges 'sa(sss)' sesman.ini 1 Sessions Policy UBC
expect_fail "non-admin cannot terminate sessions" "not authorized" rdpuser3 TerminateSession s c1
expect_fail "sudo member without interactive auth refused" "not authorized" rdpuser2 ControlService s restart

echo "--- 2. Configuration validation, backup, rollback"
expect_fail "MaxSessions=0 rejected on 0.9.24" "deny every login" urdmadmin ApplyConfigChanges 'sa(sss)' sesman.ini 1 Sessions MaxSessions 0
expect_fail "unknown key rejected" "not a supported setting" urdmadmin ApplyConfigChanges 'sa(sss)' sesman.ini 1 Xorg param /bin/sh
expect_fail "internal key rejected" "dedicated operation" urdmadmin ApplyConfigChanges 'sa(sss)' sesman.ini 1 Globals DefaultWindowManager /tmp/x.sh
expect_fail "legacy TLS rejected" "not allowed" urdmadmin ApplyConfigChanges 'sa(sss)' xrdp.ini 1 Globals ssl_protocols TLSv1.1
expect_fail "path traversal in file name rejected" "Unknown configuration file" urdmadmin ApplyConfigChanges 'sa(sss)' ../shadow 1 a b c
before=$(srv md5sum /etc/xrdp/sesman.ini | cut -d' ' -f1)
r=$(call urdmadmin ApplyConfigChanges 'sa(sss)' sesman.ini 2 Sessions MaxSessions 9999 Sessions Policy Default)
backup=$(echo "$r" | sed -n 's/^s "\(.*\)"$/\1/p')
[[ "$(ini sesman.ini Sessions MaxSessions)" == "MaxSessions=9999" && -n "$backup" ]] && ok "unlimited sentinel written, backup $backup" || bad "apply MaxSessions=9999 -> $r"
[[ "$(srv md5sum /var/lib/ubuntu-rdp-manager/backups/$backup | cut -d' ' -f1)" == "$before" ]] && ok "backup is byte-identical to previous file" || bad "backup content differs"
srv grep -q '^;; Policy - session allocation policy' /etc/xrdp/sesman.ini && ok "comments preserved" || bad "comments lost"
expect_ok "restore backup" urdmadmin RestoreBackup ss sesman.ini "$backup"
[[ "$(srv md5sum /etc/xrdp/sesman.ini | cut -d' ' -f1)" == "$before" ]] && ok "rollback restored original bytes" || bad "rollback mismatch"
expect_ok "re-apply unlimited sessions" urdmadmin ApplyConfigChanges 'sa(sss)' sesman.ini 1 Sessions MaxSessions 9999

echo "--- 3. Desktop selection"
expect_fail "unknown desktop rejected" "Unknown desktop" urdmadmin ConfigureDesktop s gnome-shell
expect_ok "configure Xfce start script" urdmadmin ConfigureDesktop s xfce
[[ "$(ini sesman.ini Globals DefaultWindowManager)" == "DefaultWindowManager=/opt/ubuntu-rdp-manager/share/startwm/startwm-xfce.sh" ]] && ok "DefaultWindowManager points at packaged script" || bad "DefaultWindowManager: $(ini sesman.ini Globals DefaultWindowManager)"
# remove per-user ~/.xsession from the image so the packaged script is what starts the desktop
srv bash -c 'rm -f /home/rdpuser*/.xsession'
expect_ok "restart service" urdmadmin ControlService s restart

echo "--- 4. Sessions: two users via the packaged Xfce script"
client 1 rdpuser1; client 2 rdpuser2
s1=$(wait_session rdpuser1); s2=$(wait_session rdpuser2)
[[ -n "$s1" && -n "$s2" ]] && ok "two concurrent sessions ($s1, $s2) started by startwm-xfce.sh" || bad "sessions did not start (s1=$s1 s2=$s2)"
sleep 5
r=$(call urdmadmin GetClientAddresses)
[[ "$r" == *"172.18."* ]] && ok "client addresses resolved from kernel socket tables: $r" || bad "client addresses: $r"
d1=$(srv loginctl show-session "$s1" -p Display --value)
expect_ok "disconnect $s1 via xrdp-dis" urdmadmin DisconnectSession s "$s1"
sleep 3
srv pgrep -u rdpuser1 -x Xorg >/dev/null && ok "rdpuser1 session survives disconnect ($d1)" || bad "rdpuser1 Xorg gone after disconnect"
docker exec urdm-cli1 pgrep -x xfreerdp3 >/dev/null && bad "client 1 still connected" || ok "client 1 was disconnected"
docker exec urdm-cli2 pgrep -x xfreerdp3 >/dev/null && ok "client 2 unaffected by disconnecting client 1" || bad "client 2 dropped"
expect_fail "terminate refuses non-xrdp session ids" "does not exist" urdmadmin TerminateSession s c999
expect_ok "terminate $s1" urdmadmin TerminateSession s "$s1"
sleep 3
srv pgrep -u rdpuser1 -x Xorg >/dev/null && bad "rdpuser1 Xorg still running" || ok "rdpuser1 session ended"
docker exec urdm-cli2 pgrep -x xfreerdp3 >/dev/null && ok "client 2 still connected after terminating session 1" || bad "client 2 dropped"
# A desktop that already ended but left programs running (logind state "closing"): logind's own
# TerminateSession ignores it on systemd 255, so the helper must stop the leftovers itself.
cg=$(srv sh -c "find /sys/fs/cgroup -type d -name session-$s2.scope | head -1")
srv su rdpuser2 -s /bin/sh -c "setsid sleep 99999 >/dev/null 2>&1 &"; sleep 1
srv sh -c "pgrep -u rdpuser2 -x sleep -n > $cg/cgroup.procs"
srv pkill -u rdpuser2 -x xfce4-session; sleep 8
[[ "$(srv loginctl show-session "$s2" -p State --value)" == closing ]] && ok "$s2 ended with a leftover program (closing)" || bad "$s2 is not closing"
expect_ok "clean up ended $s2" urdmadmin TerminateSession s "$s2"
srv pgrep -u rdpuser2 -x sleep >/dev/null && bad "leftover program of $s2 still running" || ok "leftover program of $s2 stopped"
srv loginctl show-session "$s2" >/dev/null 2>&1 && bad "$s2 still listed" || ok "$s2 gone from logind"

echo "--- 5. Group-based access control"
expect_fail "empty group refused (lockout prevention)" "empty group" urdmadmin SetAccessRestriction bsas true rdpusers 0
expect_ok "restrict to rdpusers={rdpuser3}" urdmadmin SetAccessRestriction bsas true rdpusers 1 rdpuser3
srv getent group rdpusers | grep -q rdpuser3 && ok "group created with rdpuser3" || bad "group not created"
expect_ok "restart service" urdmadmin ControlService s restart
client 3 rdpuser3; client 4 rdpuser4
s3=$(wait_session rdpuser3); sleep 8
[[ -n "$s3" ]] && ok "member rdpuser3 can log in ($s3)" || bad "member rdpuser3 could not log in"
[[ -z "$(session_of rdpuser4)" ]] && ok "non-member rdpuser4 denied" || bad "non-member rdpuser4 got a session"
srv journalctl -u xrdp-sesman --no-pager -o cat --since "-1min" | grep -q "login denied for user rdpuser4" && ok "sesman logged 'login denied for user rdpuser4'" || bad "no denial log line"
expect_fail "removing last member refused" "last member" urdmadmin SetUserAccess sb rdpuser3 false
expect_ok "grant rdpuser4" urdmadmin SetUserAccess sb rdpuser4 true
expect_ok "disable restriction" urdmadmin SetAccessRestriction bsas false "" 0
[[ -z "$(ini sesman.ini Security TerminalServerUsers)" ]] && ok "TerminalServerUsers removed" || bad "TerminalServerUsers still present"

echo "--- 6. Service control and autostart"
expect_ok "disable autostart" urdmadmin ControlService s disable
[[ "$(srv systemctl is-enabled xrdp)" == disabled ]] && ok "xrdp.service disabled" || bad "xrdp still enabled"
expect_ok "enable autostart" urdmadmin ControlService s enable
[[ "$(srv systemctl is-enabled xrdp)" == enabled ]] && ok "xrdp.service enabled" || bad "xrdp not enabled"
expect_ok "stop service" urdmadmin ControlService s stop
sleep 1
srv bash -c 'ss -ltn | grep -q ":3389 "' && bad "port 3389 still listening after stop" || ok "port 3389 closed after stop"
docker exec urdm-cli4 sh -c 'nc -z -w 3 urdm-srv 3389' && bad "TCP connect succeeded while stopped" || ok "clients cannot connect while stopped"
expect_ok "start service" urdmadmin ControlService s start
sleep 1
srv bash -c 'ss -ltn | grep -q ":3389 "' && ok "port 3389 listening after start" || bad "port 3389 not listening"

echo "--- 7. TLS certificate management"
expect_fail "invalid CN rejected" "not a valid host name" urdmadmin GenerateSelfSignedCertificate si "bad host" 365
expect_ok "generate self-signed certificate" urdmadmin GenerateSelfSignedCertificate si urdm-srv 365
srv stat -c '%U:%G %a' /etc/xrdp/urdm/key.pem | grep -q '^root:xrdp 640$' && ok "key is root:xrdp 0640" || bad "key perms $(srv stat -c '%U:%G %a' /etc/xrdp/urdm/key.pem)"
srv su -s /bin/sh xrdp -c 'test -r /etc/xrdp/urdm/key.pem' && ok "xrdp user can read key" || bad "xrdp cannot read key"
srv su -s /bin/sh rdpuser1 -c 'test -r /etc/xrdp/urdm/key.pem' && bad "ordinary user can read key" || ok "ordinary users cannot read key"
expect_fail "confused deputy: cannot import root-only key" "read permission" urdmadmin InstallCertificate ss /etc/xrdp/urdm/cert.pem /etc/ssl/private/ssl-cert-snakeoil.key

echo "--- 8. Journal fallback"
r=$(call urdmadmin ReadJournal ix 20 0)
[[ "$r" == *"_SYSTEMD_UNIT"* ]] && ok "ReadJournal returns journal JSON" || bad "ReadJournal: ${r:0:200}"

echo "--- Audit log"
srv cat /var/log/ubuntu-rdp-manager/audit.log | tail -n 60 > "$OUT/audit.log"
grep -c "outcome=" "$OUT/audit.log" | xargs echo "audit records:"
grep -q "password\|Password" "$OUT/audit.log" && bad "audit log contains password text" || ok "audit log contains no password text"

for i in 1 2 3 4; do docker rm -f "urdm-cli$i" >/dev/null 2>&1; done
echo "=== RESULT: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
