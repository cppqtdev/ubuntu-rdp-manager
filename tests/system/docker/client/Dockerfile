# Disposable headless RDP client: FreeRDP 3 rendering into Xvfb so desktops can be screenshotted.
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        freerdp3-x11 xvfb x11-apps imagemagick ca-certificates netcat-openbsd procps \
    && apt-get clean && rm -rf /var/lib/apt/lists/*
COPY rdp-connect.sh /usr/local/bin/rdp-connect.sh
RUN chmod 0755 /usr/local/bin/rdp-connect.sh
CMD ["sleep", "infinity"]
