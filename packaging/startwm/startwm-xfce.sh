#!/bin/sh
# Desktop start script for xrdp sessions (sesman.ini DefaultWindowManager), installed by
# Ubuntu RDP Manager. Runs as the logged-in user. Starts Xfce only for RDP sessions; the
# local console desktop and the system-wide x-session-manager alternative are untouched.

if test -r /etc/profile; then
    . /etc/profile
fi
if test -r "$HOME/.profile"; then
    . "$HOME/.profile"
fi

# Each RDP session is a separate X display; make sure desktop components identify it as such.
export XDG_SESSION_TYPE=x11
export XDG_CURRENT_DESKTOP=XFCE
export XDG_SESSION_DESKTOP=xfce
export DESKTOP_SESSION=xfce

# Give every RDP desktop its own D-Bus session bus. With dbus-user-session (pulled in by
# xfce4) all of a user's sessions would otherwise share /run/user/<uid>/bus, and a second
# concurrent Xfce session of the same user exits immediately because the first one already owns
# the session-manager name (verified on Ubuntu 24.04, xrdp 0.9.24, Xfce 4.18).
if command -v dbus-run-session >/dev/null 2>&1; then
    unset DBUS_SESSION_BUS_ADDRESS
    exec dbus-run-session -- startxfce4
fi
exec startxfce4
