# Pristine Ubuntu 24.04 with systemd, D-Bus and polkit but WITHOUT xrdp — the starting point of a
# fresh installation test (Test A). Desktop libraries come in through the .deb's dependencies.
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive container=docker
RUN apt-get update && apt-get install -y --no-install-recommends \
        systemd systemd-sysv dbus polkitd pkexec sudo iproute2 procps ca-certificates libpam-systemd \
        xvfb xauth xdotool imagemagick python3 \
    && apt-get clean && rm -rf /var/lib/apt/lists/*
RUN systemctl mask systemd-udevd.service systemd-udevd-kernel.socket systemd-udevd-control.socket getty@tty1.service console-getty.service
COPY create-test-users.sh /usr/local/sbin/create-test-users.sh
COPY entrypoint.sh /usr/local/sbin/entrypoint.sh
RUN chmod 0755 /usr/local/sbin/*.sh && /usr/local/sbin/create-test-users.sh \
    && useradd -m -s /bin/bash -G sudo,adm urdmadmin
STOPSIGNAL SIGRTMIN+3
ENTRYPOINT ["/usr/local/sbin/entrypoint.sh"]
