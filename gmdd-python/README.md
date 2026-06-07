## A 'double diode' Guacamole-guacd communication setup

This folder contains the Docker Compose setup and bridging scripts to make communication between Guacamole and guacd possible over data diodes. The diodes ensure only one data flow is used on each link. UDP traffic is used, so that no acknowledgements are required. To run the project, do the following things:
1. Connect two Ethernet cables and data diodes between two Linux machines (may also work without data diodes);
2. Run a setup script that configures IP addresses on the used interfaces of the cables, and correctly configure ARP;
3. Run the container configuration, causing all Guacamole traffic to be bridged over two unidirectional UDP channels.

## 1. Connect the cables

Connect two Ethernet cables between the two Linux machines. Make sure you remember the interface name, like `enp78s0` (shown when running the `ip address show` command).

## 2. Run and configure the setup

Create a file, for example `routing.yml` that holds the IP and MAC address configurations. See `routing-example.yml` for an example. The `current-node` and `next-node` variables are used to name the system you are configuring and the other system that is receiving. They need two IP addresses; one for sending and one for receiving. And for each IP address for inbound UDP packets, the MAC address needs to be known (ARP requests do not receive responses over a unidirectional channel, after all). The `setup.py` script removes all IP addresses and ARP entries for both interfaces before configuration. Then run the script.

```
sudo python3 setup.py routing.yml
```

## 3. Run the container configuration

The guacd container waits a certain amount of time before exiting when it does not receive a handshake from Guacamole. So, both configurations should be started at around the same time. The Guacamole container will initiate the handshake, so the hbridgeadapter (high side bridge adapter) needs to be running a little before that.

```
docker compose up
```

# Note: this code is not fully working

UDP packets could arrive out of order, and this code currently provides no mechanism to sequence the UDP packets. When testing, it was found that entering a valid command over a Guacamole SSH connection automatically terminates the connection, since packets had arrived out of order. But a basic SSH connection is possible with this code.
