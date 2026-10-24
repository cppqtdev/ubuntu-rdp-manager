#!/bin/sh
# Usage: rdp-connect.sh <host> <user> <display-number> [extra xfreerdp3 args...]
# Starts an Xvfb display and connects with FreeRDP 3 using full certificate verification
# (the test CA is installed into the client trust store by the test harness).
set -eu
HOST="$1"; USER_NAME="$2"; DISP="$3"; shift 3
Xvfb ":$DISP" -screen 0 1280x800x24 >/tmp/xvfb-$DISP.log 2>&1 &
sleep 1
DISPLAY=":$DISP" exec xfreerdp3 "/v:$HOST" "/u:$USER_NAME" "/p:$URDM_TEST_PASSWORD" \
    /size:1280x800 /log-level:INFO "$@"
