#!/bin/sh
# With --cgroupns=private the container only sees its own cgroup subtree;
# remount it read-write so systemd can manage units, then become PID 1.
mount -o remount,rw /sys/fs/cgroup 2>/dev/null || true
exec /lib/systemd/systemd "$@"
