# docker-compose

In order to test the data-diode software, these docker-compose files are supplied. You are able to test the software on a 1-node, 2-node and 3-node environment. For the 2-node and 3-node, the docker compose file also implements the different networks. Every compose file is required to be executed on a seperate node. So, for the 2- or 3-node architectures, you need 2 or 3 seperate (virtual) machines. If you also add data-diodes between the different machines, you are able to test the solution fully!

The data flows always in the same way. The Guacamole server (or the nettest tool, or your own client) connects to the `gmlbroker`. From there the traffic goes over the data-diode(s) to the `gmguard`, which only forwards what is allowed (keystrokes and mouse movements). Then it arrives at the `gcdbroker` that talks to `guacd`. The `guacd` does the real remote access to the `sshd` or `rdp` test server. The return path (video) goes back from the `gcdbroker` to the `gmlbroker`.

So the chain is always:

```
Guacamole / nettest  ->  gmlbroker  -->  gmguard  -->  gcdbroker  <->  guacd  ->  sshd / rdp
```

The `-->` are the places where a data-diode is placed. The only difference between the compose files is on how many machines you spread these dockers.

Below the different files are explained. Every file has a comment on top with the exact commands to test (`docker compose ... config`) and to run it (`docker compose ... up -d --build`).

## 1-node

File: `compose-1-node.yml`

This is the easiest way to start. Everything runs on one host. When you have docker installed, you get the full software including the Guacamole application and the test systems (`sshd` and `rdp`) on a single machine. So you do not have any hassel to setup a test environment yourself. There are no data-diodes here, all the dockers talk to each other inside docker. This is only for testing and to see the software working, it is not a real data-diode setup!

After it is up you can reach:

- Guacamole: http://localhost:8080 => login with `guacadmin` / `guacadmin`
- nettest: http://localhost:8081
- gmlbroker: http://localhost:4823

To test SSH and RDP you have to add a connection in the Guacamole server:

- SSH with host `sshd`, user `tester1`, password `testpass`
- RDP with host `rdp`, user `tester1`, password `testpass`, and select the option "Ignore server certificate"

Tip: on the `gmguard` in this file you can set `GUARD_APPROVE` to `deny` to test the denied path (so what happens when the guard rejects the traffic).

## 2-node

Files: `compose-2-node-low.yml` and `compose-2-node-high.yml`

This is the two-node data-diode architecture. You need two (virtual) machines. On the low-side machine you run `compose-2-node-low.yml` and on the high-side machine you run `compose-2-node-high.yml`. Between the two machines you place the data-diodes (anti-parallel). If you use real data-diodes, you have to update your local ARP table on both machines, so the machines can send the UDP packets to each other.

- **low-side** (`compose-2-node-low.yml`): runs the Guacamole webapp, the postgres database, the `gmlbroker` and the `nettest` tool. If you already have your own Guacamole server, you can remove the Guacamole and postgres part here. It is also possible to enable TLS on the `gmlbroker`, so the traffic between the Guacamole server and the `gmlbroker` is encrypted (see the TLS chapter below).
- **high-side** (`compose-2-node-high.yml`): runs the `gmguard`, the `gcdbroker`, `guacd` and the `sshd` / `rdp` test servers.

The URLs (http://localhost:8080 etc.) are the same as in the 1-node setup, but they are on the low-side machine.

## 3-node

Files: `compose-3-node-low.yml`, `compose-3-node-guard.yml` and `compose-3-node-high.yml`

This is the three-node data-diode architecture and the most secure one. Now you need three (virtual) machines. The difference with the 2-node is that the `gmguard` gets its own machine in the middle, between two data-diodes. When the `gmguard` is placed between two data-diodes, the complexity to compromise the system is significantly increased. Same as the 2-node, if you use real data-diodes you have to update the ARP table on the machines.

- **low-side** (`compose-3-node-low.yml`): runs the Guacamole webapp, the postgres database, the `gmlbroker` and the `nettest` tool. Same as with the 2-node, you can remove the Guacamole part if you already have your own server, and you can enable TLS on the `gmlbroker` (see the TLS chapter below).
- **guard-side** (`compose-3-node-guard.yml`): runs only the `gmguard`. This machine sits in the middle between the two data-diodes.
- **high-side** (`compose-3-node-high.yml`): runs the `gcdbroker`, `guacd` and the `sshd` / `rdp` test servers.

The URLs are again the same and are on the low-side machine.

## Filling in the IP addresses (only for 2-node and 3-node)

For the 1-node this is not needed, because all the dockers run on the same host and they find each other by the docker service name (like `gmguard`, `gmlbroker`, `gcdbroker`).

But when you spread the dockers over 2 or 3 machines, these names do not work anymore, because the target docker is running on an other machine. So you have to fill in the real IP address of the other machine yourself. In the config files you find the places where you have to do this, they are marked with a comment like:

```
### FILL_IN_THE_IP_ADDRESS_OF_LOW_SIDE ###
```

Replace the docker service name on that line with the IP address of the machine where that docker is running. This is what you have to change:

**2-node:**

- `compose-2-node-low.yml`: `UDP_SEND_IP` => IP address of the **high-side** machine (where `gmguard` runs).
- `compose-2-node-high.yml`: `UDP_SEND_IP` => IP address of the **low-side** machine (where `gmlbroker` runs).

**3-node:**

- `compose-3-node-low.yml`: `UDP_SEND_IP` => IP address of the **guard-side** machine (where `gmguard` runs).
- `compose-3-node-guard.yml`: `DST_IP` => IP address of the **high-side** machine (where `gcdbroker` runs).
- `compose-3-node-high.yml`: `UDP_SEND_IP` => IP address of the **low-side** machine (where `gmlbroker` runs).

So for example on the low-side of the 2-node it becomes something like:

```yaml
UDP_SEND_IP: 192.168.1.20   # the IP of the high-side machine
```

When you use real data-diodes, do not forget to also fill the ARP table (see the next chapter), otherwise the UDP packets still do not arrive.

## Filling the ARP table (only for real data-diodes)

This only counts for the 2-node and 3-node setups when you place real data-diodes between the machines. A data-diode only sends the traffic in one direction. This means the machines can not do the normal ARP requests to find out the MAC address of the other machine, because the answer can never come back over the diode. So you have to fill the ARP table yourself (static), otherwise the UDP packets will never arrive on the other side.

You need to add a static ARP entry on every machine that sends UDP over a diode, so it knows the MAC address of the machine on the other side. Do this for each direction of the diode.

On Linux you can do it like this (change the IP and MAC to your own):

```bash
sudo arp -s 192.168.1.20 aa:bb:cc:dd:ee:ff
```

Or with the newer `ip` command:

```bash
sudo ip neigh add 192.168.1.20 lladdr aa:bb:cc:dd:ee:ff dev eth0
```

Some notes:

- Do this on **both** machines of a diode (for the 3-node you have two diodes, so on the low, guard and high machine, each for the direction it sends to).
- The entry is gone after a reboot. If you want it permanent you have to add it to your startup (for example a systemd service or a network config), otherwise you have to set it again every time.
- You can check the ARP table with `arp -n` or `ip neigh` to see if the entry is there.

## TLS on the gmlbroker

The link between the Guacamole server (or your own client) and the `gmlbroker` is a normal TCP connection. On the 1-node this is all inside docker, but on the 2-node and 3-node this link can become a real network hop, for example when your Guacamole server is on an other machine than the `gmlbroker`. On that link you can turn on TLS, so the traffic between the Guacamole server and the `gmlbroker` is encrypted.

### Turn it on

You only have to do one thing on the `gmlbroker`: set `GMLBROKER_TLS` to `1` in the low-side compose file (it is `0` by default, so plaintext):

```yaml
GMLBROKER_TLS: "1"
```

That is it for the `gmlbroker`. The nice thing is that you do NOT have to generate any keys by hand. The first time the `gmlbroker` starts with TLS on, it makes its own self-signed certificate. It writes the `cert.pem` (public) and the `key.pem` (private) in the `./tls` directory next to the compose file. It only does this when the files are not there yet, so after the first start it keeps using the same certificate. If you want to use your own certificate, you can just drop your own `cert.pem` and `key.pem` in the `./tls` directory, then the `gmlbroker` uses those and does not make a new one. And if you want a fresh certificate, delete the `./tls` directory and start again.

Important to understand: the `gmlbroker` makes its certificate completely on its own, it does NOT need Guacamole for that. It also has no dependency on Guacamole, so this works the same if your Guacamole runs on the same machine or somewhere else.

### Make Guacamole trust the certificate

The only extra work is on the Guacamole side, and this is the same story no matter where your Guacamole runs. Because the certificate is self-signed, two things have to be arranged on Guacamole:

1. Guacamole must talk TLS to the `gmlbroker`. In Guacamole this is the `guacd-ssl: true` setting (or the `GUACD_SSL` environment variable on the docker image).
2. Guacamole must trust the self-signed certificate. For that you import the `cert.pem` into a Java truststore and point the Guacamole JVM to it.

So first copy the public certificate from the `gmlbroker` machine to your Guacamole machine (only the `cert.pem`, NOT the `key.pem`):

```bash
scp ./tls/cert.pem you@your-guacamole-host:/some/path/cert.pem
```

Then on the Guacamole side you build a truststore and import the certificate in it. The easiest is to start from the JDK its own `cacerts` (so the normal public CAs still work) and add the `gmlbroker` certificate:

```bash
cp $JAVA_HOME/lib/security/cacerts truststore.jks
keytool -importcert -alias gmlbroker -file cert.pem \
        -keystore truststore.jks -storepass changeit
```

And then you point the Guacamole JVM to that truststore, for example with:

```
JAVA_OPTS=-Djavax.net.ssl.trustStore=/path/truststore.jks -Djavax.net.ssl.trustStorePassword=changeit
```

After that the traffic between your Guacamole and the `gmlbroker` is encrypted.

> Note: the `key.pem` is the private key. Keep it on the `gmlbroker` machine and do not copy it around. Only the `cert.pem` (the public one) goes to the Guacamole side.

> Note: if your Guacamole runs on the SAME machine (the local test with the Guacamole in these compose files), it is the same steps. You do the `keytool` import against the `guacamole` image its cacerts (the path in that image is `/opt/java/openjdk/lib/security/cacerts`) and you set `GUACD_SSL=true` and the `JAVA_OPTS` on the `guacamole` service.

## Which one should I use?

- Just want to see it working on your laptop? => use the **1-node**.
- Want to test with real data-diodes and separate networks? => use the **2-node**.
- Want the full and most secure setup with the guard on its own machine? => use the **3-node**.
