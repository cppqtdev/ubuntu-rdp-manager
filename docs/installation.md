# Installation guide

## 1. Install the application

```bash
sudo apt install ./ubuntu-rdp-manager_1.0.0_amd64.deb
```

apt pulls the required runtime libraries from the Ubuntu archive (about 70 packages on a minimal
system). xrdp itself is only *suggested*; the wizard installs it when you ask.

The package installs:

* the application and its Qt 6.8 runtime in `/opt/ubuntu-rdp-manager`, launcher in the menu,
  `/usr/bin/ubuntu-rdp-manager`;
* the privileged helper (D-Bus activated, not running until used), its polkit actions,
  bus policy and systemd unit;
* `/var/lib/ubuntu-rdp-manager/backups` and `/var/log/ubuntu-rdp-manager/` (audit log).

## 2. Run the wizard

Start **Ubuntu RDP Manager** as your normal user (it refuses to run as root) and open
**Installation**. Every action asks for an administrator password through the desktop's polkit
agent (members of `sudo`; cached for a few minutes).

| Step | What happens |
|---|---|
| 1–4 System | Ubuntu release and architecture, administrator rights, helper presence |
| 5–8 Components | Select xrdp, xorgxrdp, Xfce (and optionally the FreeRDP 3 test client) → *Install selected*. `apt-get` output is streamed into the window. Existing configuration files are kept (`--force-confold`). |
| 9 Desktop | *Use Xfce* — RDP sessions start Xfce through the packaged `startwm-xfce.sh`; the local login is unaffected |
| 10 Authentication | Linux passwords via PAM. *Disable root login* (Ubuntu allows it by default). Optionally restrict to a group on *Users & Permissions*. |
| 11 Port and TLS | *Generate certificate* (fixes Ubuntu's unreadable default key, see below) and *Require TLS* |
| 12 Session limit | *Allow unlimited sessions* — version-safe (see `multi-session.md`) |
| 13 Start | Enable at boot and start |
| 14–16 Verify | Listener check, staged RDP/TLS connection test, overall diagnostics result |

### Why step 11 matters on Ubuntu 24.04

The stock xrdp package points `/etc/xrdp/key.pem` at the snakeoil key, which the `xrdp` user
cannot read. xrdp then silently accepts only non-TLS "Standard RDP Security" connections
(no server authentication). The wizard detects this and installs a dedicated key readable only
by the `xrdp` group. Details in `compatibility.md`.

## 3. Network access

The application never opens firewall ports. Allow RDP only from trusted networks, e.g.:

```bash
sudo ufw allow from 192.168.1.0/24 to any port 3389 proto tcp
```

Prefer a VPN for access from outside the LAN; never forward 3389 from the internet.
Alternatively bind xrdp to one address in *Configuration → Network*.

## 4. Connect

Use any RDP client (Windows *Remote Desktop Connection*, FreeRDP, Remmina) to
`<server>:3389` and sign in with a Linux account. With a self-signed certificate the client shows
the certificate's SHA-256 fingerprint; compare it with *Configuration → TLS certificate* before
trusting it, or install a certificate from your organisation's CA.

## Upgrade

`sudo apt install ./ubuntu-rdp-manager_<new>.deb`. The package owns no files in `/etc/xrdp`, so
xrdp configuration, backups and the audit log are preserved; a running helper is restarted.
(Verified 1.0.0 → 1.0.1 → 1.0.2.)

## Removal

* `sudo apt remove ubuntu-rdp-manager` removes the application, helper, D-Bus and polkit files.
  **xrdp keeps running with your configuration.** If `DefaultWindowManager` still points at the
  packaged Xfce script, it is reset to xrdp's `startwm.sh` (backup:
  `/etc/xrdp/sesman.ini.ubuntu-rdp-manager-removed`) so logins keep working. A certificate in
  `/etc/xrdp/urdm/` is kept because `xrdp.ini` may reference it.
* `sudo apt purge ubuntu-rdp-manager` additionally deletes backups and the audit log.
* xrdp, xorgxrdp and Xfce are ordinary Ubuntu packages; remove them with apt if wanted.

## Installing from source

See `build.md`; `cmake --install build` installs to `/opt/ubuntu-rdp-manager` and the system
locations above, but the maintainer-script steps (state directories, D-Bus reload) must then be
done by hand. The `.deb` is the supported route.
