# Build and test validation summary

All results below were produced on 2026-10-24 on the development machine (Ubuntu 24.04.4,
14 threads, 15 GiB RAM) using disposable Docker containers with systemd as PID 1
(`--cgroupns=private`). Nothing was installed on the host. Evidence lives in
`tests/system/results/`.

## Build

| Item | Result |
|---|---|
| Compiler / flags | GCC 13.3, C++20, strict warnings + hardening — **0 warnings** (Debug and Release) |
| Qt | 6.8.8 SDK, FluentWinUI3 style verified rendering in light and dark |
| Package | `ubuntu-rdp-manager_1.0.0_amd64.deb`, 39 MB, all files `root:root`, standard modes |
| Runtime dependencies | `ldd` over every bundled binary/plugin in pristine `ubuntu:24.04` with the declared `Depends` installed → **0 unresolved** |

## Automated tests

| Suite | Result | Evidence |
|---|---|---|
| Unit tests (9 suites, ~70 test functions) | **9/9 pass** (Debug and Release) | `ctest` |
| GUI smoke test (all 11 pages, host build and installed build in container) | **0 QML warnings/errors** | `--smoke-test` |
| Helper integration (real systemd/logind/polkit/xrdp/FreeRDP 3) | **56/56 pass** | `results/final/helper-integration/` |
| GUI-driven acceptance (xdotool on the installed GUI) | **21/21 pass** | `results/final/gui-acceptance/` (screenshots of every step) |

## Acceptance tests from the specification

| Test | Status | Notes / evidence |
|---|---|---|
| A – Fresh installation | **Pass** | pristine systemd image → `apt install ./…deb` (70 deps) → GUI wizard installed xrdp/xorgxrdp/Xfce in 213 s → GUI steps configured desktop, root login, certificate, TLS, unlimited, autostart, start → 3 concurrent TLS logins with Xfce desktops |
| B – Enable/disable through the UI | **Pass** | UI stop → unit inactive, port closed, remote TCP connect refused; UI start → listening, new user reached a desktop |
| C – Two simultaneous users | **Pass** | `results/2026-10-24-two-users/` (screenshots of both desktops) |
| D – Five simultaneous users | **Pass** | `results/2026-10-24-five-users/` (5/5, ~1.8 s login, 746 MiB) |
| (D+) – Ten simultaneous users | **Pass** | minimal Xfce: `…ten-users/` (1.48 GiB); full Xfce with packaged script: `…ten-users-packaged/` (2.94 GiB, 2.6–3.5 s) |
| E – Unlimited configuration | **Pass** | UI wrote `MaxSessions=9999` (0.9.24-safe) with backup; Dashboard shows "Unlimited (configuration)" and hardware headroom separately; `MaxSessions=0` empirically blocks all logins (`…maxsessions-zero/`) |
| F – Reconnection | **Pass** | Policy=Default: disconnect via UI, reconnect → same display (`:13`); also via client kill in Phase 1 |
| G – Session isolation | **Pass (partly by design)** | separate Xorg per user, separate logind scopes; UI/helper refuse non-xrdp sessions; ordinary users cannot read the TLS key; per-user file permission checks between accounts were not separately scripted (Linux DAC) |
| H – Failure handling | **Partial** | wrong password (`AUTHFAIL` classified), invalid config (refused, file untouched), port closed after stop, missing desktop script (refused), certificate failure (FreeRDP refused untrusted/wrong-pinned cert; probe reports), insufficient permissions (polkit denial), group-restricted login refused. Not performed: real port conflict with another daemon, resource exhaustion |
| I – Performance reporting | **Pass** | `performance.md`; RAM/CPU/load/login time recorded per run |

## Package lifecycle

| Step | Result |
|---|---|
| Install 1.0.0 on pristine system | pass; xrdp only suggested, not installed |
| Upgrade 1.0.0 → 1.0.1 → 1.0.2 | pass; `/etc/xrdp` untouched (package owns no files there), backups and audit log kept, helper restarted |
| Remove | pass; app, helper, D-Bus and polkit files gone; xrdp keeps serving; dangling `DefaultWindowManager` reset with backup |
| Purge | pass; `/var/lib` and `/var/log` state removed |

## Findings discovered during validation (and fixed or handled)

1. `MaxSessions=0` blocks all logins on 0.9.24 → version-aware limit policy.
2. Stock Ubuntu silently downgrades to non-TLS → diagnostics + wizard fix.
3. Restart orphans desktops → Orphaned state, warnings, cleanup.
4. Same-user second desktop exits with `dbus-user-session` → private bus in packaged script.
5. FluentWinUI3 drew dark controls in a light theme when deployed (no platform theme honours the
   request on Linux) → gtk3 platform theme + start-up `GTK_THEME`, matched token rule.
6. QML role named `state` shadowed `Item.state` → renamed.
7. Uninstall would have left sesman pointing at a removed script → postrm reset.
8. Two `/proc/net/tcp` decoding bugs caught by fixture-based unit tests.

## Not validated

See `known-issues.md` → *Not validated* (real polkit dialog, physical networks, Windows clients,
redirection, >10 users, other Ubuntu releases).
