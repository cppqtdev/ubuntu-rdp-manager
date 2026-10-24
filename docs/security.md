# Security

## Threat model (summary)

* Local unprivileged users must not be able to change the RDP server, read other users' session
  details beyond what the OS already exposes, or abuse the root helper.
* An administrator acting through the GUI must authenticate (polkit) for every class of change,
  and every change must be auditable and reversible.
* Remote attackers must not get a weaker RDP configuration than the administrator intended.

## Privilege separation

| Component | Runs as | Can |
|---|---|---|
| `ubuntu-rdp-manager` (GUI) | the logged-in user; refuses root | read world-readable state; ask the helper |
| `urdm-helper` | root, D-Bus activated, exits after 120 s idle | only the 14 methods of `com.urdm.Helper1` |

* Only root may own `com.urdm.Helper1` (bus policy). Callers are identified by their unique bus
  name; polkit resolves it to a process and user (no PID-based subjects).
* Each method maps to one polkit action (`com.urdm.*`), all `auth_admin_keep` for active local
  sessions and denied for inactive/remote sessions. Verified: non-admin → denied; sudo member
  without an agent → denied; admin → allowed.
* There is no generic command/file method. Programs are executed with absolute paths, an
  argument list (never a shell), an empty environment plus `PATH`/`LANG`, stdin from
  `/dev/null`, and timeouts.

## Input validation (enforced in the helper, mirrored in the GUI)

| Input | Rule |
|---|---|
| Config file | only `xrdp.ini` / `sesman.ini` (no paths) |
| Config key | must be in `ConfigSchema`, not internal-only |
| Config value | per-type validation; no newlines/NUL (no INI injection); `MaxSessions=0` refused on 0.9; `security_layer=rdp` and pre-1.2 TLS refused |
| Session id | `[A-Za-z0-9]{1,32}`; session must exist and have `Service=xrdp-sesman` |
| User / group | POSIX name regex; user must exist and have UID ≥ 1000 |
| Paths | absolute, normalised, no `..`, regular file (no symlinks, `O_NOFOLLOW` reads), size limit, readable by the caller (confused-deputy protection) |
| Packages | fixed component → package map; only `apt-get update` / `install` |
| Backup id | strict pattern, must belong to the named file |

Unit tests cover these rules with a fake system layer (`tests/unit/tst_helpercore.cpp`); the
integration test exercises them against real polkit/systemd/logind.

## File handling

* Configuration writes: backup → atomic write (mode preserved) → verify → restore on mismatch.
* Helper-written files use `mkostemp` + `fchmod 0600` before writing content + `fchown/fchmod` +
  `fsync` + `rename`.
* TLS private key: `root:xrdp 0640`; certificate `0644`.
* Audit log `/var/log/ubuntu-rdp-manager/audit.log`: `root:adm 0640`, append-only opens with
  `O_NOFOLLOW`; also written to the journal (`SYSLOG_IDENTIFIER=urdm-helper`). Every request is
  recorded with uid, user, operation, arguments and outcome — including denied ones. Secrets are
  redacted (`LogSanitizer`).
* The packaged desktop script is executed for every user, so `ConfigureDesktop` refuses it unless
  it is root-owned and not group/world writable.

## Secure defaults the wizard applies

* Dedicated TLS key readable by xrdp (fixes Ubuntu 24.04's silent non-TLS fallback).
* `security_layer=tls` (a broken key then fails loudly instead of downgrading).
* Root login disabled.
* Optional group restriction with lockout protection.
* No firewall changes and no exposure by default; the UI warns when RDP listens on non-loopback
  addresses and recommends LAN/VPN-only access.

## Secrets

* Passwords are never requested, stored, logged or passed to QML. Authentication is PAM inside
  xrdp-sesman; the GUI only triggers polkit, whose agent handles the password.
* The connection test does not authenticate. "Launch RDP client" starts the client without
  credentials; users sign in at the xrdp login screen.
* Exports (logs, diagnostics bundle) are sanitized: password/secret/token key-value pairs,
  FreeRDP `/p:`, `{base64}` credentials, bearer tokens and PEM private keys are redacted. Private
  key files are never included. User names and client IPs *are* included (stated in the bundle).

## Certificate verification

* The connection test never ignores certificate errors, except that for the local machine it
  accepts a certificate whose SHA-256 is identical to the configured one (pinning).
* "Launch RDP client" passes `/cert:fingerprint:sha256:<configured cert>` for the local machine
  (verified: a wrong pin is rejected by FreeRDP 3) and uses normal verification otherwise.

## Hardening of the helper unit

`PrivateTmp`, `ProtectHome=read-only`, `ProtectKernelTunables/Logs`, `ProtectClock`,
`ProtectHostname`, `RestrictRealtime`, `RestrictNamespaces`, `LockPersonality`,
`SystemCallArchitectures=native`, `RestrictAddressFamilies=AF_UNIX AF_NETLINK AF_INET AF_INET6`.
Not enabled, because package installation must work: `ProtectSystem`, `NoNewPrivileges`,
`RestrictSUIDSGID`. Binaries are built with stack protector, `_FORTIFY_SOURCE=3`, full RELRO, PIE;
the helper uses an absolute RPATH.

## Residual risks / items for review

* Bundled Qt must be kept up to date (security fixes); rebuild the package for new Qt patch
  releases.
* `auth_admin_keep` caches authorization for a few minutes per session (polkit default).
* An administrator can still configure insecure xrdp options by editing files directly; the
  Diagnostics page reports the main ones.
* xrdp 0.9 has no NLA; credentials are sent inside TLS after the server is authenticated — the
  certificate must therefore be verified by clients.
