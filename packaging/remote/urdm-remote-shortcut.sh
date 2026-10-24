#!/bin/sh
# Ubuntu RDP Manager - puts a "Disconnect" icon on the desktop of remote (xrdp) sessions, once per
# user. Deleting the icon is respected: it is not created again.
[ -n "$XRDP_SESSION" ] || exit 0
marker="${XDG_CONFIG_HOME:-$HOME/.config}/ubuntu-rdp-manager/disconnect-shortcut-created"
[ -e "$marker" ] && exit 0
src=/usr/share/applications/urdm-disconnect.desktop
[ -r "$src" ] || exit 0
desk=$(xdg-user-dir DESKTOP 2>/dev/null)
[ -n "$desk" ] && [ "$desk" != "$HOME" ] || desk="$HOME/Desktop"
mkdir -p "$desk" "$(dirname "$marker")" || exit 0
dst="$desk/urdm-disconnect.desktop"
if [ ! -e "$dst" ]; then
    cp "$src" "$dst" && chmod 755 "$dst"
    # Xfce 4.18 only runs desktop launchers it trusts: a checksum stored in gvfs metadata.
    if command -v gio >/dev/null 2>&1; then
        sum=$(sha256sum "$dst" | cut -d' ' -f1)
        gio set -t string "$dst" metadata::xfce-exe-checksum "$sum" 2>/dev/null || true
    fi
fi
touch "$marker"
