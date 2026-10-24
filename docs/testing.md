# Testing

Nothing in the test suite modifies the development host. Everything that touches xrdp, systemd,
polkit or packages runs in disposable Docker containers.

## 1. Unit tests (Qt Test, no privileges)

```bash
ctest --test-dir build --output-on-failure
```

| Suite | Covers |
|---|---|
| `tst_inidocument` | byte-exact round trip of Ubuntu's real `xrdp.ini`/`sesman.ini`, case-insensitive keys, duplicate keys, CRLF, insertion/removal |
| `tst_sessionlimit` | Debian version parsing; `MaxSessions` semantics for 0.9 vs 0.10; sentinel; schema rejection of 0 on 0.9 |
| `tst_configschema` | every setting type, secure-default rules, INI injection, listen-spec grammar, validators (users, groups, paths, session ids, displays, hosts) |
| `tst_configstore` | backup + apply + permissions, invalid change leaves file untouched, internal keys, rollback, traversal-proof backup ids, pruning, missing files |
| `tst_procparsers` | `/proc` parsers incl. **real captures** of `/proc/net/unix` (two attached displays) and `/proc/net/tcp6`; IPv4/IPv6/mapped address decoding |
| `tst_misc` | log sanitizer, resource advisor, service-state mapping |
| `tst_rdpnegotiation` | X.224/RDP_NEG request encoding and response parsing (TLS, fallback, failure, malformed) |
| `tst_logclassifier` | classification of **real xrdp 0.9.24 journal lines** and a real journal capture |
| `tst_helpercore` | all helper operations against a fake system layer: whitelists, non-xrdp sessions refused, lockout prevention, confused-deputy checks, key permissions, install plan, version-aware config |

## 2. GUI smoke test

```bash
xvfb-run -a ./build/src/app/ubuntu-rdp-manager --smoke-test
```
Instantiates every page and exits non-zero if any QML warning or error was logged.

## 3. Helper integration test (real systemd, logind, polkit, xrdp, FreeRDP 3)

```bash
tests/system/make-test-pki.sh /tmp/pki urdm-srv
DESTDIR=/tmp/stage cmake --install build
tests/system/start-test-server.sh /tmp/pki /tmp/stage
URDM_PKI=/tmp/pki tests/integration/helper-integration.sh results/integration
```
56 checks: authorization (non-admin, admin without agent, admin), config validation, backup
byte-identity, rollback, desktop configuration, two real concurrent sessions, client-address
resolution, disconnect/terminate with bystander unaffected, group restriction denying a real
login, autostart, stop closes the port, TLS key permissions, confused-deputy refusal, journal
fallback, audit log contents.

The container uses a **test-only** polkit rule granting `urdmadmin` (there is no polkit agent to
type a password); production uses the shipped `auth_admin_keep` defaults.

## 4. GUI-driven acceptance test

`tests/system/gui-acceptance.sh` runs the installed GUI inside the server container under Xvfb
and operates it with `xdotool`, verifying every effect independently (systemd, sockets,
`sesman.ini`, logind, FreeRDP clients). 21 checks: stop/start through the UI (port closed,
remote connect refused, new login after start), autostart switch, unlimited sessions, orphan
cleanup, disconnect with bystander, **reconnect to the same display**, terminate, orphan reporting.

## 5. Load / concurrency tests

```bash
tests/system/run-multisession-test.sh <N> results/users-N
```
N independent FreeRDP 3 clients log in as N accounts simultaneously; records per-user login
time, display, screenshots of every desktop, logind data, memory and load.

## 6. Everything at once

```bash
tests/system/run-all.sh build results [--with-load]
```

## 7. Package lifecycle (performed manually in a pristine container; see validation summary)

1. `docker run … urdm-ubuntu-base:24.04` (systemd, no xrdp).
2. `apt install ./ubuntu-rdp-manager_1.0.0_amd64.deb` → dependencies resolved, xrdp not pulled in.
3. GUI → Installation → *Install selected* → xrdp/xorgxrdp/Xfce installed by the helper.
4. GUI → steps 9–15 → verify files, permissions, service state.
5. Concurrent real logins.
6. `apt install` of a higher version → config, backups, audit log preserved.
7. `apt remove` → app gone, xrdp still serving; `apt purge` → state removed.

## 8. Manual test procedures (real hardware / real clients)

Record date, Ubuntu version, client and result for each.

| # | Procedure | Expected |
|---|---|---|
| M1 | Fresh Ubuntu 24.04 desktop VM: install the .deb, run the wizard with the real polkit password dialog | Password prompt appears for each action class; all wizard steps green |
| M2 | Windows `mstsc` to the server; verify certificate fingerprint matches *Configuration → TLS* | Login screen of xrdp, Xfce desktop |
| M3 | Two different Windows/Linux clients, two accounts at the same time | Two independent desktops; files of one user not readable by the other (`ls ~other` → permission denied) |
| M4 | Wrong password | Login refused; Logs → category *auth-failure*; Dashboard counter increases |
| M5 | Pull the client's network cable for 60 s, reconnect | Session shows Disconnected, then Connected again on the same display |
| M6 | Users → restrict to group with one member; second account tries to log in | Refused; *Last denied login* shows it |
| M7 | Configuration → change port to 3390, save, restart; connect to 3390 | Works; 3389 closed |
| M8 | Generate certificate, connect with a client that pinned the old one | Client warns about changed certificate |
| M9 | Session Limits → maximum 1; two users log in | Second refused; Logs → *limit-denied* |
| M10 | Stop server with users logged in | Confirmation shows counts; after start, old desktops shown as Orphaned; Clean up ends them |
| M11 | Diagnostics → Export bundle; inspect | No passwords or private keys; README states what is included |
| M12 | Keyboard only: navigate all pages with Ctrl+1…0, Tab, Space/Enter | All controls reachable |
| M13 | Resolutions 1280×720, 1366×768, 1920×1080, 2560×1440 | No clipped controls; sidebar collapses below 1100 px |

## 9. Not covered by automation

Real polkit agent dialogs, physical networks/WAN, Windows/macOS/mobile clients, audio and drive
redirection, sessions lasting hours, more than 10 users, hosts other than Ubuntu 24.04 x86_64.
