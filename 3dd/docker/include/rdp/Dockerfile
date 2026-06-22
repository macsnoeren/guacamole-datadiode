FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive

# xrdp is the RDP server; xorgxrdp is its Xorg backend; xfce4 gives it a desktop
# session to serve; dbus-x11 lets that session start cleanly. Same tester
# credentials as the sshd container so both backends behave alike.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        xrdp xorgxrdp xfce4 xfce4-terminal dbus-x11 \
    && rm -rf /var/lib/apt/lists/*

# User tester1
RUN useradd -m -s /bin/bash tester1 \
 && echo 'tester1:testpass' | chpasswd \
 && echo 'xfce4-session' > /home/tester1/.xsession \
 && chown tester1:tester1 /home/tester1/.xsession

# User tester2
RUN useradd -m -s /bin/bash tester2 \
 && echo 'tester2:testpass' | chpasswd \
 && echo 'xfce4-session' > /home/tester2/.xsession \
 && chown tester2:tester2 /home/tester2/.xsession

# User tester3
RUN useradd -m -s /bin/bash tester3 \
 && echo 'tester3:testpass' | chpasswd \
 && echo 'xfce4-session' > /home/tester3/.xsession \
 && chown tester3:tester3 /home/tester3/.xsession

RUN adduser xrdp ssl-cert \
 && dbus-uuidgen --ensure

EXPOSE 3389
COPY start-xrdp.sh /usr/local/bin/start-xrdp.sh
RUN chmod +x /usr/local/bin/start-xrdp.sh

# Run xrdp in the foreground (as PID 1) so it gets SIGTERM on container stop.
CMD ["/usr/local/bin/start-xrdp.sh"]
