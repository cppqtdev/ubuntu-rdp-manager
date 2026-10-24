# Architecture

## System view

```
 RDP clients (Windows mstsc, FreeRDP, Remmina, thin clients)
        │  RDP over TLS (port 3389 by default)
        ▼
 ┌──────────────────────────── Ubuntu host ─────────────────────────────┐
 │  xrdp.service ──Requires──▶ xrdp-sesman.service (BindsTo xrdp)       │
 │      │                          │ PAM (common-auth) + logind session  │
 │      │                          ▼                                     │
 │      │            per user: Xorg :N (xorgxrdp) + startwm-xfce.sh      │
 │      │            → dbus-run-session → Xfce   (session-cN.scope)      │
 │                                                                       │
 │  ubuntu-rdp-manager (GUI, unprivileged)                               │
 │      reads: systemd + logind (D-Bus), /proc, cgroup v2, /etc/xrdp,    │
 │             journald (journalctl), dpkg-query                         │
 │      writes: nothing — every change goes to the helper ─┐             │
 │                                                          ▼            │
 │  urdm-helper (root, D-Bus activated, com.urdm.Helper1)                │
 │      polkit CheckAuthorization per call → validate → act → audit      │
 │      systemd jobs · ConfigStore (backup/atomic/rollback) · logind     │
 │      xrdp-dis · groupadd/gpasswd · openssl · apt-get (fixed plan)     │
 └───────────────────────────────────────────────────────────────────────┘
```

The RDP protocol, authentication and session creation are entirely xrdp's. This project adds
the management plane.

## Layers

| Layer | Location | Responsibility |
|---|---|---|
| UI | `qml/` | FluentWinUI3 screens (`screens/`), reusable components (`components/`), design tokens (`themes/Theme.qml`). No business logic; binds to C++ properties and calls invokables. |
| Application / view models | `src/app/services/` | `RdpServerManager`, `RdpConfigurationManager`, `SessionManager`, `UserAccessManager`, `SystemMonitor`, `LogManager`, `DiagnosticManager`, `InstallationManager`; composed by `ApplicationController` (the single QML singleton `App`). |
| Models | `src/app/models/`, models inside services | `QAbstractListModel`s for sessions, settings, users, logs (+ a filter proxy). |
| Infrastructure | `src/app/infrastructure/` | `SystemdClient`, `LogindClient`, `HelperClient` (PrivilegedOperationClient), `ProcessRunner`, `SystemSampler`, `RdpProbe`. |
| Domain / core | `src/core/` | Pure, unit-tested logic shared by GUI and helper: `IniDocument`, `ConfigSchema`, `ConfigStore`, `SessionLimitPolicy`, `XrdpVersion`, `ListenSpec`, `Validators`, `ProcParsers`, `ResourceAdvisor`, `ServiceState`, `LogClassifier`, `LogSanitizer`, `RdpNegotiation`, `HelperProtocol`. |
| Privileged operations | `src/helper/` | `HelperService` (D-Bus adaptor), `PolkitAuthorizer`, `HelperCore` (all validation and actions, written against the `SystemOps` interface), `LinuxSystemOps`, `PackageJob`, `AuditLog`. |

Dependencies are passed through constructors; `ApplicationController` is the composition root.
There are no global service singletons apart from the one QML entry point.

## Verified data sources (no invented APIs)

| Information | Source |
|---|---|
| Service state, autostart, uptime, main PID | `org.freedesktop.systemd1` unit properties, `PropertiesChanged` after `Manager.Subscribe` |
| Sessions (id, user, display, start time, leader, scope) | `org.freedesktop.login1` sessions with `Service=xrdp-sesman` |
| Connected vs disconnected | `/proc/net/unix`: `/run/xrdp/sockdir/xrdp_display_N` has a connected socket only while a client is attached |
| Orphaned | session opened before the running `xrdp-sesman` started, or while it is stopped (sesman keeps sessions only in memory) |
| Per-session memory / CPU / process count | cgroup v2 `user.slice/user-UID.slice/session-ID.scope/{memory.current,cpu.stat,cgroup.procs}` |
| Client address (on demand, root) | `sock_diag` peer of the display socket → owning xrdp process (`/proc/<pid>/fd`) → its TCP peer in `/proc/net/tcp6` |
| Listener / port conflicts | `/proc/net/tcp{,6}` LISTEN sockets vs configured ports and unit state |
| CPU, memory, load, pressure, uptime | `/proc/stat`, `/proc/meminfo`, `/proc/loadavg`, `/proc/pressure/memory`, `/proc/uptime` |
| Versions | `dpkg-query` |
| Events | `journalctl -u xrdp -u xrdp-sesman -o json`, classified with patterns verified on 0.9.24 |

Session control uses upstream mechanisms only: `xrdp-dis` (disconnect) and logind
`TerminateSession` (terminate), after the helper re-checks that the session belongs to
`xrdp-sesman`.

## Helper D-Bus interface `com.urdm.Helper1` (`/com/urdm/Helper1`)

| Method | polkit action | Notes |
|---|---|---|
| `Version() → s` | none | |
| `ControlService(s action) → s` | `com.urdm.service.control` | start/stop/restart (systemd job awaited) or enable/disable (both unit files) |
| `ApplyConfigChanges(s file, a(sss) changes) → s backupId` | `com.urdm.config.modify` | only schema-listed, non-internal keys; version-aware validation |
| `RestoreBackup(s file, s id)` | `com.urdm.config.modify` | current file is backed up first |
| `InstallCertificate(s cert, s key)` / `GenerateSelfSignedCertificate(s cn, i days)` | `com.urdm.config.modify` | key stored `root:xrdp 0640`; caller must be able to read the source files |
| `ConfigureDesktop(s id)` | `com.urdm.config.modify` | `xfce` or `system-default`; script must be root-owned |
| `DisconnectSession(s id)` / `TerminateSession(s id)` | `com.urdm.sessions.manage` | refuses non-xrdp sessions |
| `GetClientAddresses() → a{sv}` | `com.urdm.sessions.inspect` | display → address |
| `SetAccessRestriction(b, s group, as members)` / `SetUserAccess(s user, b)` | `com.urdm.access.manage` | refuses empty group / removing the last member |
| `StartPackageInstall(as components) → s jobId` | `com.urdm.packages.install` | fixed component → package map; signals `JobOutput`, `JobFinished` |
| `ReadJournal(i max, x since) → s` | `com.urdm.logs.read` | for users outside `adm`/`systemd-journal` |

There is no generic "run command" or "write file" method.

## Threading and responsiveness

* All subprocesses (`dpkg-query`, `journalctl`, `tar`) run through `ProcessRunner` (async, timeout,
  killed on exit). The GUI never calls `waitForFinished`.
* D-Bus calls from the GUI are asynchronous (`QDBusPendingCallWatcher`); helper calls allow
  interactive authorization so the desktop's polkit agent can ask for a password.
* `/proc` and cgroup sampling runs on a worker thread (`QtConcurrent`) every 2 s (configurable),
  skipping a tick if the previous sample is still running.
* systemd and logind updates are event-driven; slow safety refreshes (15 s) cover missed signals.
* Screens are created on first visit and kept.

## Theming

FluentWinUI3 is selected with `QQuickStyle::setStyle` and imported in `Main.qml` (so it is
deployed). Qt 6.8 cannot switch light/dark at runtime on Linux, so the preference is applied at
start-up through the gtk3 platform theme (`GTK_THEME`), and `Theme.dark` uses FluentWinUI3's own
rule so custom colours always match the controls. Changing the theme offers a restart.

## Packaging layout

```
/opt/ubuntu-rdp-manager/{bin,libexec,lib,plugins,qml,share}   app, helper, bundled Qt 6.8.8
/usr/bin/ubuntu-rdp-manager                                   symlink
/usr/share/dbus-1/system-services/com.urdm.Helper1.service    D-Bus activation
/usr/share/dbus-1/system.d/com.urdm.Helper1.conf              bus policy (root owns the name)
/usr/lib/systemd/system/urdm-helper.service                   hardened Type=dbus unit
/usr/share/polkit-1/actions/com.urdm.policy                   7 actions, auth_admin_keep
/var/lib/ubuntu-rdp-manager/backups/                          config backups
/var/log/ubuntu-rdp-manager/audit.log                         audit trail (root:adm 0640)
```
