FROM ubuntu:24.04
RUN apt-get update \
    && apt-get install -y --no-install-recommends openssh-server \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir -p /run/sshd \
    && ssh-keygen -A \
    && echo 'PasswordAuthentication yes' >> /etc/ssh/sshd_config

# User tester1
RUN useradd -m -s /bin/bash tester1 \
 && echo 'tester1:testpass' | chpasswd

# User tester2
RUN useradd -m -s /bin/bash tester2 \
 && echo 'tester2:testpass' | chpasswd

# User tester3
RUN useradd -m -s /bin/bash tester3 \
 && echo 'tester3:testpass' | chpasswd

EXPOSE 22
# -D: stay in the foreground; -e: log to stderr so `docker logs sshd` shows
# guacd's auth attempts when debugging the flow.
CMD ["/usr/sbin/sshd", "-D", "-e"]
