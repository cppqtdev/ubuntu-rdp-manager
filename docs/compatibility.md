# Compatibility report (Phase 1)

Date of verification: 2026-10-24. All results below were produced in a **disposable**
Docker environment (`tests/system/docker/`), never on the developer host.

## Target platform

| Item | Verified value |
|---|---|
| OS | Ubuntu 24.04.4 LTS (noble), x86_64 |
| xrdp | `0.9.24-4ubuntu0.1` (Ubuntu archive) |
| xorgxrdp | `1:0.9.19-1` |
| Desktop | Xfce 4.18 (`xfce4`) |
| Test client | FreeRDP `3.32.1+dfsg-0ubuntu0.24.04.1` (`freerdp3-x11`, `xfreerdp3`) |
| init | systemd 255 |
| GUI toolkit | Qt 6.8.8 (`~/Qt/6.8.8/gcc_64`), FluentWinUI3 Qt Quick Controls style |
| Compiler / build | GCC 13.3, CMake 3.28, Ninja 1.11 |

ogon was evaluated earlier and **not selected**: ogon 1.0.0 (last commit 2021) requires
FreeRDP 2.0, a patched Thrift and Qt 5 components, and is AGPL-3.0. xrdp satisfies every
mandatory requirement (see below), so ogon is not used.

## Verified xrdp facts that drive the design

### 1. `MaxSessions=0` is NOT "unlimited" on xrdp 0.9.x

* `sesman.ini(5)` for 0.9.24 claims *"If not set or set to 0, unlimited sessions are allowed"*.
* The 0.9.24 code (`sesman/session.c`) does `if (g_session_count >= g_cfg->sess.max_sessions) deny`,
  and display allocation iterates `display - offset <= max_sessions`.
* **Empirical result:** with `MaxSessions=0`, every login is refused:
  `xrdp-sesman: [ERROR] max concurrent session limit exceeded. login for user rdpuser1 denied`
  (`tests/system/results/2026-10-24-maxsessions-zero/`).
* xrdp 0.10.x (`sesman/scp_process.c`) checks `max_sessions > 0 && count >= max_sessions`, so 0 *is*
  unlimited there.

**Consequence:** the configuration adapter is version-aware (`SessionLimitPolicy`):

| xrdp version | "Unlimited (configuration)" is written as |
|---|---|
| `< 0.10` | `MaxSessions=9999` (sentinel; displays `:10`..`:10009`) |
| `>= 0.10` | `MaxSessions=0` |

When reading, `MaxSessions=0` on 0.9.x is reported as an **error state** ("all logins denied"),
and values `>= 9999` are reported as "Unlimited (configuration)".
Ubuntu's packaged default is `MaxSessions=50`.

### 2. Service units

* `xrdp.service` (Type=forking, `User=xrdp`) `Requires=xrdp-sesman.service`.
* `xrdp-sesman.service` is `BindsTo=xrdp.service` + `StopWhenUnneeded=true`.
* Therefore `xrdp.service` is the single control point for start/stop/restart; enable/disable is
  applied to both unit files so they stay consistent.
* sesman listens on `[::1]:3350` (`ListenAddress=127.0.0.1` in Ubuntu's file resolves to loopback).

### 3. Session discovery and control (no invented APIs)

| Need | Mechanism | Privilege | Verified |
|---|---|---|---|
| List sessions | logind `ListSessions` filtered by `Service=xrdp-sesman` (`Display`, `Name`, `Leader`, `Timestamp`) | none | yes |
| Connected vs disconnected | `/proc/net/unix`: `…/sockdir/xrdp_display_N` has an established (`St=03`) socket only while an RDP client is attached | none | yes |
| Disconnect a session | upstream `xrdp-dis` (sends a datagram to `xrdp_disconnect_display_N`), run as root with `DISPLAY=:N` | root (helper) | yes — client dropped, Xorg `:11` kept |
| Terminate a session | logind `TerminateSession(id)` after checking `Service=xrdp-sesman` | root (helper) | yes — sesman logged `++ terminated session` and cleaned the sockets |
| Client address | `/proc/<xrdp child>/fd` inode → `/proc/net/tcp6` peer | root (helper) | partially (requires `CAP_SYS_PTRACE`; not available in the unprivileged test container) |
| `xrdp-sesadmin` | needs a **username and password** for sesman's PAM login in 0.9.24 | — | rejected (GUI must not handle passwords) |

### 4. Session allocation policies (0.9.24)

`Policy=Default|UBD|UBI|UBC|UBDI|UBDC` (no `Separate` until 0.10).

* `Default` — one session per *(user, bpp)*: reconnecting reattaches to the existing display (**verified**:
  rdpuser1 reconnected to `:10`, `reconnectwm.sh` ran).
* `UBC` — session per connection: same user can hold several independent desktops (**verified**:
  rdpuser1 on `:10` and `:11` simultaneously, separate session D-Bus daemons). Trade-off: a reconnect
  never reattaches, so disconnected sessions accumulate unless `KillDisconnected`/`DisconnectedTimeLimit`
  are set. On hosts where `dbus-user-session` is installed the per-user bus is shared between the
  user's sessions, which can confuse desktop components — documented in `multi-session.md`.

### 5. Access control

`TerminalServerUsers=<group>` is enforced **only if the group exists**; Ubuntu's file names `tsusers`
but does not create it, so by default every user that passes PAM may log in. `AlwaysGroupCheck=true`
makes a missing group deny everyone (fail closed). The app manages RDP access through this group.

### 6. Desktop selection

`DefaultWindowManager` accepts an absolute path (`sesman/config.c`). The app ships
`startwm-xfce.sh` and points `DefaultWindowManager` at it, instead of changing the system-wide
`x-session-manager` alternative (which would also change local logins).

### 7. TLS — stock Ubuntu 24.04 silently falls back to non-TLS

xrdp runs as user `xrdp`. Its postinst links `/etc/xrdp/key.pem` to
`/etc/ssl/private/ssl-cert-snakeoil.key` (`root:ssl-cert 0640`) but does **not** add `xrdp` to
`ssl-cert`. Verified on a pristine container, the journal then shows:

```
[ERROR] Cannot read private key file /etc/xrdp/key.pem: Permission denied
[WARN ] Cannot accept TLS connections because certificate or private key file is not readable.
[INFO ] Security protocol: configured [RDP], requested [SSL|HYBRID|HYBRID_EX|RDP], selected [RDP]
[INFO ] Non-TLS connection established from ::ffff:172.18.0.3 ...
```

i.e. every client is silently downgraded to Standard RDP Security (no server authentication,
MITM-able). The application detects this (Diagnostics → "TLS key readable by xrdp") and the setup
wizard fixes it by installing a dedicated certificate/key (`root:xrdp 0640`) and setting
`security_layer=tls` so a broken key fails loudly instead of downgrading.
With a test CA trusted by the client, FreeRDP 3 connected **with full certificate verification**
(no `/cert:ignore`).

### 8. Stopping or restarting xrdp orphans running desktops

Verified 2026-10-24 (rdpuser6 on display `:12`):

* `systemctl stop xrdp` stops both units; the user's Xorg `:12`, desktop and logind session `c38`
  **keep running** (they live in the user's `session-*.scope`, not in the service cgroup).
* After `systemctl start xrdp`, sesman starts with an empty in-memory session list. Reconnecting as
  the same user created a **new** session on `:13`; `:12` stays alive but can no longer be reached.
* Ubuntu's `ExecStop=xrdp-sesman --kill` exits with status 1 on every stop, leaving
  `xrdp-sesman.service` in `failed` (result `exit-code`) until the next start. Harmless.

The application therefore marks sessions opened before the current sesman start (or while sesman is
stopped) as **Orphaned**, warns before stop/restart with the number of running desktops, and offers
to terminate orphaned sessions. It does not claim session persistence across service restarts.

## Multi-session results (disposable container, idle Xfce desktops)

| Test | Users | Result | Login time (simultaneous start) | Container memory (cgroup) |
|---|---|---|---|---|
| C | 2 | 2/2 PASS, displays :10–:11 | ~1.9 s | 323 MiB |
| D | 5 | 5/5 PASS, displays :10–:14 | ~1.8 s | 746 MiB |
| — | 10 | 10/10 PASS, displays :10–:19 | ~2.6–2.7 s | 1.48 GiB |

Login time = client launch → user's `xfce4-session` process present on the server. Evidence
(screenshots of every desktop, logind data, journals, per-process memory) is in
`tests/system/results/`. Per-user RSS sums (~430–510 MiB) overcount shared pages; the cgroup
figure (~148 MiB per idle session) is the realistic marginal cost. Real workloads (browsers,
office suites) cost far more — see `performance.md`.

Limits of this evidence: the container shares the host kernel; the clients ran on the same
physical machine; no WAN latency was simulated.
