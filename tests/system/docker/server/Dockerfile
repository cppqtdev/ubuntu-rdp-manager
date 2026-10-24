# Disposable Ubuntu 24.04 + systemd test host for xrdp acceptance tests.
# Never used on the developer host; see docs/testing.md.
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive container=docker
RUN apt-get update && apt-get install -y --no-install-recommends \
        systemd systemd-sysv dbus dbus-x11 polkitd pkexec iproute2 procps sudo ca-certificates \
        xrdp xorgxrdp xfce4 xfce4-terminal xfce4-session xfwm4 xfdesktop4 x11-xserver-utils \
        libpam-systemd ssl-cert fonts-dejavu-core \
    && apt-get clean && rm -rf /var/lib/apt/lists/*
# Mask units that are pointless/failing in a container.
RUN systemctl mask systemd-udevd.service systemd-udevd-kernel.socket systemd-udevd-control.socket getty@tty1.service console-getty.service
# Test-only accounts. Passwords are throwaway values for the disposable container.
COPY create-test-users.sh /usr/local/sbin/create-test-users.sh
RUN chmod 0755 /usr/local/sbin/create-test-users.sh && /usr/local/sbin/create-test-users.sh
COPY entrypoint.sh /usr/local/sbin/entrypoint.sh
RUN chmod 0755 /usr/local/sbin/entrypoint.sh
STOPSIGNAL SIGRTMIN+3
ENTRYPOINT ["/usr/local/sbin/entrypoint.sh"]
