FROM ubuntu:24.04
RUN apt-get update \
    && apt-get install -y --no-install-recommends openssh-server \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir -p /run/sshd \
    && ssh-keygen -A \
    && useradd -m -s /bin/bash tester \
    && echo 'tester:testpass' | chpasswd \
    && echo 'PasswordAuthentication yes' >> /etc/ssh/sshd_config
EXPOSE 22
# -D: stay in the foreground; -e: log to stderr so `docker logs sshd` shows
# guacd's auth attempts when debugging the flow.
CMD ["/usr/sbin/sshd", "-D", "-e"]
