#!/bin/sh
# Creates throwaway accounts rdpuser1..rdpuser10 inside the disposable test container.
# The shared test password comes from tests/system/test-credentials.env (never real credentials).
set -eu
PASS="${URDM_TEST_PASSWORD:-UrdmTest-2026!}"
for i in $(seq 1 10); do
  u="rdpuser$i"
  id "$u" >/dev/null 2>&1 || useradd -m -s /bin/bash "$u"
  echo "$u:$PASS" | chpasswd
  :
  :
done
