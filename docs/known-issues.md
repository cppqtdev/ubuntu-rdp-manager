# Known issues and limitations

## Upstream / platform behaviour (verified, handled by the application)

| # | Issue | Handling |
|---|---|---|
| 1 | xrdp 0.9.24: `MaxSessions=0` refuses every login although the man page says "unlimited" | "Unlimited" written as `MaxSessions=9999` on 0.9.x; 0 refused; existing 0 flagged |
| 2 | Ubuntu 24.04: xrdp cannot read its default snakeoil key → silent fallback to non-TLS Standard RDP Security | Diagnostics check; wizard installs a dedicated key (`root:xrdp 0640`) and sets `security_layer=tls` |
| 3 | Stopping/restarting xrdp leaves desktops running but sesman forgets them (not reconnectable) | Sessions shown as **Orphaned**; warnings with counts before stop/restart; cleanup action |
| 4 | Ubuntu's `xrdp-sesman` `ExecStop` exits 1 on every stop → unit shows `failed` until next start | Displayed as a harmless stop, not as a fault |
| 5 | With `dbus-user-session`, a second concurrent Xfce desktop of the same user exits immediately | Packaged start script gives each desktop a private session bus (verified fix). Trade-off: those desktops don't share the systemd user bus. Removing the package reverts to stock behaviour. |
| 5b | A user signed in locally on a Wayland desktop (Ubuntu GNOME) who also connects remotely got a **black screen**: the remote session shares `/run/user/<uid>`, GTK found the local `wayland-0` socket and Xfce's desktop/settings daemons crashed (found on real hardware, RK3328 client, 2026-10-24) | Packaged start script forces X11 (`GDK_BACKEND=x11`, `QT_QPA_PLATFORM=xcb`, `CLUTTER_BACKEND=x11`, unset `WAYLAND_DISPLAY`) — fixed in 1.0.2 |
| 5c | Same user signed in locally and remotely: both desktops share one PipeWire instance. `pipewire-module-xrdp` makes `xrdp-sink` the default for the whole user (local sound goes to the remote client) and forces a global quantum; with the graph clocked by local hardware at 48 kHz and xrdp at 44.1 kHz, remote sound is choppy. Found on real hardware 2026-10-24 | Health Check warning (same account) mentions sound; Settings → *Sound tips*. Use a separate account for remote work |
| 5d | FreeRDP `/sound:sys:alsa,latency:700` buffers ~0.7 s on the client: sound lags and keeps playing after the source stops | Client-side; *Sound tips* recommend `/sound:sys:pulse` or `latency:150` |
| 6 | Qt 6.8 cannot change light/dark at runtime on Linux | Theme applied at start-up via the gtk3 platform theme; changing it offers a restart |
| 7 | Root is required to map sessions to client IPs | Loaded on demand through the helper (one authorization) |

## Limitations of this release

* Session controls are limited to what xrdp 0.9 supports: disconnect (`xrdp-dis`), terminate
  (logind). No shadowing, messaging or per-session resolution control.
* "Disconnected since" is known only for disconnections observed while the application was running.
* Log-derived counters (failed logins, start failures, …) depend on journal retention and on
  message formats verified for xrdp 0.9.24; other versions may need new patterns.
* Printer, USB device (urbdrc) and camera (rdpecam) redirection, the GFX pipeline (RemoteFX
  progressive / H.264), RD Gateway and NLA are not available in xrdp 0.9. They are listed as
  "Not supported by this server" on the Settings page.
* The diagnostics check for keys under `/etc/ssl/private` infers readability from `ssl-cert`
  group membership because an unprivileged process cannot inspect that directory.
* `KillDisconnected`/`DisconnectedTimeLimit` and `IdleTimeLimit` are written and validated, but
  their timing behaviour was not measured in tests.
* The package is amd64 only; arm64 was not built or tested.
* The bundled Qt runtime includes Qt Quick Controls styles that are not used (Material, Imagine,
  Universal, …) — about 10 MB that could be trimmed.
* When the gtk3 platform theme is active, file dialogs are GTK dialogs.
* Package metadata contains placeholders: maintainer address and homepage
  (`packaging/CMakeLists.txt`), and the application's own licence (`LICENSES/`).

## Not validated (by design of the test environment)

* A real polkit authentication dialog (containers have no polkit agent; a test-only rule stood in).
* Physical hardware, real LAN/WAN conditions, Windows/macOS/mobile RDP clients.
* Audio and drive redirection end-to-end.
* More than 10 simultaneous users; sessions lasting hours or days; active workloads.
* Ubuntu releases other than 24.04.
* Accessibility with a screen reader (Accessible names/roles are set; not tested with Orca).

## Observed but not explained

* Once (1 of 3 attempts) a first login with Ubuntu's **stock** `startwm.sh` (after the package had
  been removed) exited immediately; it did not reproduce. With the packaged start script this was
  not observed in any run.

## Test-environment artefacts (not product issues)

* Docker's Ubuntu image contains `policy-rc.d`, so xrdp is not auto-started by its postinst inside
  containers; on a normal system it is.
* GTK prints AT-SPI warnings when no accessibility bus exists (headless containers).
