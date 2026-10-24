#!/bin/sh
# Ubuntu RDP Manager - "Disconnect" inside a remote desktop.
# Closes the connection but keeps the desktop and its programs open, exactly like closing the
# remote desktop window on the other computer. Connecting again returns to the same desktop.
if [ -z "$XRDP_SESSION" ] || ! command -v xrdp-dis >/dev/null 2>&1; then
    if command -v zenity >/dev/null 2>&1; then
        zenity --info --title="Disconnect" --text="This is not a remote desktop, so there is nothing to disconnect." 2>/dev/null
    fi
    exit 0
fi
exec xrdp-dis
