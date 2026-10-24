# Multi-user sessions and "unlimited" sessions

## How independent desktops work

Every successful RDP login is handled by `xrdp-sesman`, which opens a PAM session (a logind
session in the user's `session-*.scope`), starts a dedicated Xorg server with the xorgxrdp driver
on its own display (`:10`, `:11`, …) and runs the desktop start script as that user. Users
therefore get separate X servers, processes, environments and file permissions; one user cannot
see or control another's desktop.

Verified: 2, 5 and 10 different accounts simultaneously, each with its own desktop, both with a
minimal Xfce and with the full `xfce4` package as installed by the wizard (`validation-summary.md`).

## Allocation policy (sesman `Policy`)

| Policy | New desktop when… | Reconnect behaviour |
|---|---|---|
| `Default` | user (and colour depth) differs | returns to the user's running desktop (**verified**) |
| `UBC` / `UBDC` | every connection | never returns to an earlier desktop |
| `UBD`, `UBI`, `UBDI` | user + display size / client IP differ | returns when attributes match |

xrdp 0.9.24 has no `Separate` policy (added in 0.10).

### Same user, several desktops

Supported with `UBC`, verified on a standard install, **with the packaged start script**:
`startwm-xfce.sh` gives each desktop a private D-Bus session bus (`dbus-run-session`).
Without it — with Ubuntu's stock `startwm.sh` and `dbus-user-session` (installed with `xfce4`) —
all of a user's desktops share `/run/user/<uid>/bus` and the second concurrent Xfce session
exits immediately (verified). Trade-off of the private bus: desktop programs do not see the
systemd user bus, so user services started there are not shared between that user's desktops.
GNOME Shell does not support concurrent sessions of one user at all; use Xfce.

With `UBC`, disconnected desktops are never reused: enable *End disconnected sessions* with a
time limit, or clean them up on the Sessions page.

## "Unlimited sessions"

| xrdp version | Unlimited is written as | Why |
|---|---|---|
| 0.9.x (Ubuntu 24.04) | `MaxSessions=9999` | 0.9.24's `session.c` compares `count >= max_sessions`; with `MaxSessions=0` **every login is refused** (verified, contrary to its man page) |
| ≥ 0.10 | `MaxSessions=0` | 0.10 treats 0 as no limit |

The helper refuses `MaxSessions=0` on 0.9.x, and the UI flags an existing `MaxSessions=0` as
"blocks all logins". Ubuntu's default is `MaxSessions=50`.

**Unlimited is a configuration statement, not a capacity promise.** Each desktop costs memory
(measured: ~150 MiB idle minimal Xfce, ~300 MiB idle full Xfce with its services) and CPU once
users work. The *Session Limits* page shows a live estimate of additional desktops and can
turn it into a hard limit (*Use capacity-based limit*).

## Other limits that apply even when "unlimited"

* RAM and swap; CPU (software rendering — xorgxrdp has no GPU acceleration).
* X display numbers: `X11DisplayOffset` (10) + up to 9999 sessions; each display also needs its
  sockets in `/tmp/.X11-unix` and `/run/xrdp/sockdir`.
* Per-user process limits (`ulimit -u`, the systemd `TasksMax=` of `user-UID.slice`), file
  descriptors — relevant when one account holds many desktops.
* Disk space for home directories and `/tmp`.
* Network bandwidth (not measured in this project; depends heavily on screen content).

## Disconnect, reconnect and restarts

| Event | What happens (xrdp 0.9.24, verified unless marked) |
|---|---|
| Client closes / network drops | Session becomes *Disconnected*; desktop keeps running; with `Default` policy the user returns to it on next login |
| Admin *Disconnect* | Same as above (`xrdp-dis`) |
| Admin *Terminate* | All programs of that session end (logind) |
| `KillDisconnected` + limit | sesman ends the session after the time limit (documented by xrdp; not separately timed in tests) |
| xrdp stop / restart | Desktops keep running but sesman forgets them → **Orphaned**; next login creates a new desktop. Terminate orphans from the Sessions page. |
| Desktop crash (window manager exits) | sesman ends the session (observed: "Window manager … exited quickly") |
| OS reboot | All sessions end (not persisted) |
