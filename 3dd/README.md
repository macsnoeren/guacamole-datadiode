Iron Bridge: foundation of the 'remote access over a Triple Data Diode architecture' use case.

Project Iron Bridge created the initial PoC for a secure system that regulates the flow of Guacamole remote access traffic between networks. It was created to give control and security over data streams to the OT (DCS) side. This increases plant operator control over inbound requests. It provides strict filtering on inbound traffic.

![Iron Bridge server rack](docs/rack.webp)

## Architecture

In total, five C++ CLI programs ('apps') were developed to make the use case possible (excluding Guacamole and remote access apps). Three of these are necessary to run:
- **gmlbroker**, Guacamole broker. The interface through which the web server receives and sends remote access traffic. It sits on the non-critical side of the network (plant DMZ) and routes remote access traffic over the 3DD. Runs on: low side send proxy.
- **guard**. This application filters out any traffic not strictly required for remote access. This includes file transfer operations. It also serves as the approving/denying barrier for remote connections. Runs on: guard proxy.
- **gcdbroker**, guacd broker. The interface through which the guacd program receives and sends remote access traffic. Runs on: high side send proxy.

On the hardware topology level, there are two network paths, one inbound (into the plant) and one outbound. It is physically impossible to reverse the network stream direction on these paths, due to data diodes. Simplified, the paths visit:

Inbound: web server <--> Guacamole broker -->(DD) guard -->(DD) guacd broker <--> guacd
and
Outbound: guacd <--> guacd broker -->(DD) Guacamole broker <--> web server
where DD is one data diode.

Two receiver proxies were created (low side receive proxy & high side receive proxy) to route traffic from guacd broker and from the guard respectively. We currently do not see much use in these proxies, as their presence does not contribute to security much, and can be left out.

The full architecture:

![Full 3DD architecture](docs/5node-arch.webp)

## Environment

Each app runs inside its own Docker container. Some Docker Compose configurations were made for convenient testing and running the system. Currently, the following configurations exist inside 3dd/docker:
- 1node: Run all applications on a single machine
- 3node: Run necessary applications on three nodes: the low node (web server + broker), the guard node (guard proxy), and the high node (guacd + broker). Needs additional network (IP address + ARP entry) configuration to work
- 5node: Same as 3node, but also runs the optional proxies on two different nodes. Also needs additional network configuration to work.
- bridgeless: a pure web server-to-guacd configuration with no 3DD apps involved.

## Installation

git clone the repository. For a 3 node configuration, connect up each nodeas shown in the architecture diagram with ethernet cables. Then, configure each node to have its own IP address. For nodes that SEND over a data diode, the address which they are sending to needs to be hardcoded. Normally, it can discover addresses using ARP, but a pure data diode does not allow this to happen.

### Network configuration

There are two ways to do network configuration: manually or automated. Manual setup on all three nodes requires:
```
# IP addresses
sudo ip addr add <send-ip> dev <send-interface>
sudo ip link set <send-interface> up
sudo ip addr add <receive-ip> <receive-interface>
sudo ip link set <receive-interface> up

# Set ARP entry for sending interface
sudo ip neigh add <ip-to-send-to> lladdr <mac-address-to-send-to> nud permanent dev <send-interface>
```

However, there is a setup script that wipes IP addresses and ARP entries on an interface and sets new ones automatically (3dd/setup/setup.py). It uses a YAML config for the placeholder values. See 3dd/setup/3node-routing-example.yml for an example. The setup script will automatically apply the values with:
```
sudo python3 setup.py config.yml
```

When running the Guacamole container for the first time, initialize the postgres db with the command:

```
docker run --rm guacamole/guacamole:1.6.0 /opt/guacamole/bin/initdb.sh --postgresql | \
    docker exec -i guacamole_db psql -U guacamole_user -d guacamole_db -f -
```
