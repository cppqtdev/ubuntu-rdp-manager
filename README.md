# Ubuntu RDP Manager

A Qt 6.8 / QML (FluentWinUI3) desktop application that enables and manages **multi-user remote
desktop (RDP) access on Ubuntu** using the distribution's own **xrdp + xorgxrdp** packages.
Several Linux users can work in independent, simultaneous desktops; the configured session limit
can be set to unlimited while the application tracks how much the hardware can really carry.

The application does not implement RDP. It installs, configures, monitors and controls xrdp
through a small, polkit-authorized root helper and never runs its GUI as root.

## What it does

| Area | Highlights |
|---|---|
| Installation wizard | Detects OS/arch/admin rights, installs `xrdp`, `xorgxrdp`, Xfce from the Ubuntu archive, configures desktop, TLS, session limit, starts and verifies the server |
| Server control | Start / stop / restart / autostart of `xrdp.service` (sesman follows it), live state from systemd |
| Sessions | Live list from logind (+ kernel socket state + cgroup usage): connected, disconnected, **orphaned**; disconnect, terminate, clean up abandoned sessions; client IPs on demand |
| Session limits | "Unlimited (configuration)" that is safe on xrdp 0.9 (where `MaxSessions=0` blocks everyone), allocation policy, idle/disconnect timeouts, capacity estimate and warnings |
| Users | Restrict RDP to a Linux group (sesman `TerminalServerUsers`), per-user allow/revoke, lockout protection |
| Configuration | Schema-validated editor for verified xrdp/sesman keys, TLS certificate generation/import, desktop choice, backups and one-click rollback |
| Performance | CPU, memory, PSI, per-session memory/CPU from cgroups, start/failure counts |
| Logs & diagnostics | journald viewer with search/severity/category filters, sanitized export, ~20 health checks with fixes, staged RDP connection test, diagnostics bundle |

## Verified on

Ubuntu 24.04.4 LTS (x86_64), xrdp 0.9.24-4ubuntu0.1, xorgxrdp 0.9.19, Xfce 4.18, FreeRDP 3.32
clients, Qt 6.8.8 — in disposable Docker/systemd containers. **10 simultaneous users** each with
their own desktop were verified, as was the same user holding two desktops. See
[docs/validation-summary.md](docs/validation-summary.md).

## Quick start

```bash
sudo apt install ./ubuntu-rdp-manager_1.0.0_amd64.deb
```

Then start **Ubuntu RDP Manager** from the application menu (or `ubuntu-rdp-manager`) as a normal
administrator account and follow *Installation*. Each change asks for your password (polkit).

## Building

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.8/gcc_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
cd build && cpack -G DEB
```

Details: [docs/build.md](docs/build.md).

## Repository layout

```
src/core/        shared, Qt-Core-only library: INI round-tripping, config schema & validation,
                 version-aware session limits, ConfigStore (backup/atomic write/rollback),
                 /proc parsers, log classification & sanitizing, RDP negotiation codec
src/helper/      urdm-helper: root D-Bus service com.urdm.Helper1 (polkit, audit, validation)
src/app/         ubuntu-rdp-manager GUI: services/view-models, models, infrastructure clients
qml/             FluentWinUI3 screens, components and theme tokens
packaging/       polkit policy, D-Bus activation/policy, systemd unit, startwm script, Debian
tests/unit/      Qt Test suites (core + helper logic with fakes)
tests/integration/  helper against real systemd/logind/polkit/xrdp (container)
tests/system/    disposable server/client images, multi-user load, GUI-driven acceptance
docs/            architecture, installation, user guide, security, licensing, results
```

## Documentation

* [Architecture](docs/architecture.md) · [Build](docs/build.md) · [Installation](docs/installation.md)
* [User guide](docs/user-guide.md) · [Configuration](docs/configuration.md) · [Multi-session](docs/multi-session.md)
* [Security](docs/security.md) · [Performance & capacity](docs/performance.md)
* [Testing & manual procedures](docs/testing.md) · [Validation summary](docs/validation-summary.md)
* [Compatibility report](docs/compatibility.md) · [Licensing](docs/licensing.md) · [Known issues](docs/known-issues.md)

## Licence

Application code: see `LICENSES/LicenseRef-Proprietary.txt` (**placeholder — the copyright
holder must set the final terms**). Bundled Qt and third-party components: see
[docs/licensing.md](docs/licensing.md).
