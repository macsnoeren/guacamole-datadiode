# Docker

This project provides different proxies that are required to implement the Remote Access Guacamole solution over a two-node or three-node data-diode architecture. These applications are also provided as Docker images on Docker Hub, so they can be directly used.

Note that it is important to set the build context correctly. See the individual README.md files for each docker for more information.

The following Docker images are created by this project:

- gmlbroker: https://hub.docker.com/r/macsnoeren/gmdatadiode-gmlbroker
- gmguard: https://hub.docker.com/r/macsnoeren/gmdatadiode-gmguard
- gcdbroker: https://hub.docker.com/r/macsnoeren/gmdatadiode-gcdbroker
- nettest: https://hub.docker.com/r/macsnoeren/gmdatadiode-nettest
- sshtest: https://hub.docker.com/r/macsnoeren/gmdatadiode-sshtest
- rdptest: https://hub.docker.com/r/macsnoeren/gmdatadiode-rdptest

There is also a `test-guacamole` image in the [test-guacamole](test-guacamole) folder. This is not a proxy and it is not published on Docker Hub. It is the normal Apache Guacamole webapp image with one small extra startup step, so it can trust the self-signed certificate that the gmlbroker makes when TLS is turned on. It is only used to test the solution.

## Deprecated

When this repository started, a first concept version was available for testing. These old Docker images can be found here:

- gmserver: https://hub.docker.com/r/macsnoeren/gmdatadiode-gmserver
- gmclient: https://hub.docker.com/r/macsnoeren/gmdatadiode-gmclient
- gmproxyin: https://hub.docker.com/r/macsnoeren/gmdatadiode-gmproxyin
- gmproxyout: https://hub.docker.com/r/macsnoeren/gmdatadiode-gmproxyout
