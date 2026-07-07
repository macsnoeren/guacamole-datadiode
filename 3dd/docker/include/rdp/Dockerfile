FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive

# xrdp is the RDP server; xorgxrdp is its Xorg backend; xfce4 gives it a desktop
# session to serve; dbus-x11 lets that session start cleanly. Same tester
# credentials as the sshd container so both backends behave alike.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        xrdp xorgxrdp xfce4 xfce4-terminal dbus-x11 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd -m -s /bin/bash tester \
    # username:password
    && echo 'tester:testpass' | chpasswd \
    && adduser xrdp ssl-cert \
    && dbus-uuidgen --ensure \
    && echo 'xfce4-session' > /home/tester/.xsession \
    && chown tester:tester /home/tester/.xsession

EXPOSE 3389
COPY start-xrdp.sh /usr/local/bin/start-xrdp.sh
RUN chmod +x /usr/local/bin/start-xrdp.sh

# Run xrdp in the foreground (as PID 1) so it gets SIGTERM on container stop.
CMD ["/usr/local/bin/start-xrdp.sh"]
