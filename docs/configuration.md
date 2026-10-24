# Configuration guide

The application edits only keys verified against xrdp 0.9.24's man pages and source
(`src/core/ConfigSchema.cpp`). Unknown keys, internal keys and unsafe values are rejected by the
helper even if a modified client sent them.

## How a change is applied

1. The GUI validates the value (same schema) and shows errors inline.
2. On **Save**, the helper re-validates for the installed xrdp version, copies the current file to
   `/var/lib/ubuntu-rdp-manager/backups/<file>.<UTC timestamp>`, writes the new file atomically
   (temp file + fsync + rename, permissions preserved), re-reads it and restores the backup if the
   result does not match.
3. Only the changed lines are touched; comments, ordering and other settings are preserved
   byte-for-byte.
4. xrdp/sesman read their files at start-up: the UI shows *Restart required*. On xrdp 0.9 a
   restart orphans running desktops, so restart when nobody is logged in.

**Rollback:** *Configuration → Backups → Restore* (the current file is backed up first, so a
restore can itself be undone). The latest 30 backups per file are kept.

## Settings

| Area | Setting | File / key | Notes |
|---|---|---|---|
| Network | Listening address and port | `xrdp.ini [Globals] port` | `3389`, `tcp://192.168.1.10:3389`, `tcp6://{fd00::1}:3389`, `tcp://.:3389` (loopback); several separated by spaces |
| Network | TCP keep-alive | `tcp_keepalive` | detects vanished clients |
| Security | Security layer | `security_layer` | `tls` recommended; `rdp` is refused (no server authentication) |
| Security | TLS protocol versions | `ssl_protocols` | only TLSv1.2 / TLSv1.3 may be enabled |
| Security | TLS cipher list | `tls_ciphers` | empty = OpenSSL default |
| Security | Standard RDP encryption level | `crypt_level` | only relevant for `negotiate` fallback |
| Security | Allow root login | `sesman.ini [Security] AllowRootLogin` | Ubuntu default `true`; disable |
| Security | Maximum login attempts | `MaxLoginRetry` | |
| Sessions | Maximum sessions / Unlimited | `[Sessions] MaxSessions` | version-aware, see `multi-session.md` |
| Sessions | Allocation policy | `Policy` | Default, UBD, UBI, UBC, UBDI, UBDC |
| Sessions | End disconnected sessions / time limit | `KillDisconnected`, `DisconnectedTimeLimit` | seconds |
| Sessions | Idle time limit | `IdleTimeLimit` | seconds, 0 = never |
| Sessions | First display | `X11DisplayOffset` | |
| Redirection | Clipboard channel | `xrdp.ini [Channels] cliprdr` | |
| Redirection | Block copy session→client / client→session | `RestrictOutboundClipboard`, `RestrictInboundClipboard` | `none`, `all` or `text,file,image` |
| Redirection | Drive redirection | `rdpdr` | appears as `~/thinclient_drives` (FUSE) |
| Redirection | Audio output and microphone | `rdpsnd` | needs `pipewire-module-xrdp` (Setup component "Sound and microphone"); microphone (audin) also needs `drdynvc` |
| Redirection | Dynamic virtual channels | `drdynvc` | microphone, dynamic resolution |
| Redirection | All channels (master switch) | `xrdp.ini [Globals] allow_channels` | `false` blocks every channel |
| Redirection | RemoteApp | `[Channels] rail` | basic support in 0.9 |
| Redirection | Mount redirected drives | `sesman.ini [Chansrv] EnableFuseMount` | |
| Redirection | Drive folder name | `[Chansrv] FuseMountName` | relative to `$HOME` or absolute; `%u`/`%U` allowed; `..` refused |
| Display | Maximum colour depth | `max_bpp` | resolution is chosen by the client |
| Display | Multiple monitors | `allow_multimon` | |
| Display | Bulk compression | `bulk_compression` | |
| Display | Fast-path | `use_fastpath` | both, input, output, none |
| Logging | xrdp / sesman log level | `[Logging] LogLevel` | CORE, ERROR, WARNING, INFO, DEBUG |
| Desktop | Desktop for RDP sessions | `sesman.ini DefaultWindowManager` | set via *Desktop* card only |
| Desktop | Per-user `~/startwm.sh` | `EnableUserWindowManager` | |
| TLS | Certificate / key | `certificate`, `key_file` | set via *Generate* / *Install* only |
| Access | RDP group | `TerminalServerUsers`, `AlwaysGroupCheck` | set via *Users & Permissions* only |

Not offered because xrdp 0.9 does not support them: printer redirection, per-session
resolution limits (the client decides), NLA/CredSSP.

## Client options (Settings → "What the connecting computer can use")

The card maps every option a client such as FreeRDP can request to the settings above. Options xrdp
0.9 cannot provide stay listed as **Not supported by this server** so people know to switch them off
on the client rather than wondering why nothing happens:

| Client option | xrdp 0.9.24 |
|---|---|
| `/sound`, `/microphone` | supported (rdpsnd / audin via `pipewire-module-xrdp`); PCM, Opus when the client offers it |
| `/drive`, `/drive:hotplug`, `/smartcard` | supported (rdpdr) |
| `/clipboard`, `/multimon`, `/dynamic-resolution`, `/app` | supported |
| `/printer` | not supported |
| `/usb` (urbdrc) | not supported — USB sticks work through drive redirection |
| `/dvc:rdpecam` (camera) | not supported |
| `/gfx`, `/gfx:AVC444` | not supported — clients fall back to the classic bitmap path automatically |
| `/sec:nla` | not supported — clients must use `/sec:nla:off` (TLS remains) |
| `/g` (RD Gateway) | not supported — use a VPN |

## TLS certificates

* **Generate self-signed:** RSA 3072, SHA-256, SAN = host name/IP, 1–825 days. Stored in
  `/etc/xrdp/urdm/cert.pem` (0644) and `key.pem` (`root:xrdp 0640`).
* **Install your own:** choose PEM certificate and unencrypted PEM key. The helper checks that
  you can read both files yourself (it will not copy another service's key on your behalf),
  rejects symlinks, checks the key matches and the certificate has not expired.
* Clients: distribute your CA, or verify the fingerprint shown in *Configuration → TLS*.

## Access control

*Users & Permissions → Only members of a group* creates the group if needed, adds the selected
users and sets `TerminalServerUsers=<group>` + `AlwaysGroupCheck=true` (missing group = deny all).
The helper refuses an empty group and refuses to remove the last member. *Every account* removes
`TerminalServerUsers` (sesman only enforces it when present) and sets `AlwaysGroupCheck=false`.
Changes apply to new logins only.
