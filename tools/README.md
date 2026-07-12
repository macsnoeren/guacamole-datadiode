# Tools

Tools and applications created to support the development of the Guacamole data-diode software.

During the research I needed several small tools to understand how the Guacamole protocol works and to test different ideas. These tools are not part of the final data-diode solution, but they helped a lot to get insight into the protocol and to experiment with the setup. I keep them here so others can use them as well and maybe learn from them.

Every tool has its own directory with its own README that explains the details. Below is a short overview so you know what you can find here.

## pyproxy

A very simple TCP/UDP proxy written in pure Python. This was the first tool I used to look at the Guacamole protocol between the Guacamole Server and guacd. Because it sits in the middle of the connection, you can see the data that is exchanged and even change it. It also implements a first simple protocol checker that checks the `LENGTH.VALUE` format of the Guacamole protocol.

The proxy is based on the work of rsc-dev (see the README inside the directory). You can easily add your own data handlers to inspect or filter the traffic, which makes it very handy while testing network applications.

See [pyproxy/README.md](pyproxy/README.md) for the usage and examples.

## gmtrafficdumper

A tool to capture all Guacamole traffic between the Guacamole web interface and guacd, and write it to a log file. The goal is to get a better insight in how the Guacamole protocol works under the hood. The logs show the opcodes and the parameter values of the protocol, so you can really follow what is happening during a session.

It uses a containerized Guacamole installation with Docker Compose and a small Python proxy that logs the messages. The README explains the full setup, including how to connect two Linux machines over Ethernet and how to create SSH and RDP connections to test it.

This part was developed by Simon de Cock (on behalf of RWE Generation SE) during his work on the project.

See [gmtrafficdumper/README.md](gmtrafficdumper/README.md) for the complete setup.

## gmdd-python

A 'double diode' setup that makes communication between Guacamole and guacd possible over data-diodes, written in Python. It uses two unidirectional UDP channels, one for each direction, so that only one data flow is used on each link and no acknowledgements are required.

The directory contains the Docker Compose files, the bridge adapters (high side and low side) and a `setup.py` script that configures the IP addresses and the ARP entries on the interfaces. This is needed because ARP requests do not get a response over a unidirectional channel.

Please note: this code is not fully working. UDP packets could arrive out of order and there is currently no mechanism to put them back in sequence. A basic SSH connection is possible, but for example entering a valid command can terminate the connection because of the out of order packets. It is more a proof of concept to show that the idea works.

See [gmdd-python/README.md](gmdd-python/README.md) for how to run it.
