![badge-development-in-progress](https://img.shields.io/badge/development-in_progress-blue)
![badge-version-0-1-test](https://img.shields.io/badge/version-0.1_(test)-yellow)

# Guacamole data-diode

While cyber criminals and nation state cyber hackers are more and more able to use zero-days in their attacks. The need for hardware-based security has born. Especially, for critical and vital computing systems, someone should not rely sololy on software based security systems. In the last year (2024) the big firewall suppliers all have had severe vulnerabilities in their products. The Danish critical infrastructure has been attack where zero-days have been used in the Zyxel equipment.

In my research for hardware-based solutions for different kind of interfaces, it shows that many vendor are still converting to software based solutions. Especially remote access is a type of interface that is not easily secured. When having the best security, still it is possible that some person with malicious intent could convince the company to login. Therefore, the hardware-based solution shall only protect the interface in such way that an attack is not able to lateral move into the critical network nor attack the critical network by means of network-based hardware. The hardware-based solution shall implement the functionality by its hardware and therefore no attacker is able to bypass by a new firmware update or a found bug in the software.

Furthermore, open source solutions are great for the society and why not trying to implement a hardware-based solution to protect the interface for an existing open source remote access solution. Guacamole from Apache is a very handy remote access solution that centralizes the remote connection and it does not expose the remote access protocols itseld. That is already very secure, however still based on software. While Guacamole has a very modular setup, it felt somehow that it would be possible to use data-diode technology to secure the remote access interface. 

This project is investigating a hardware-based security solution for the Guacamole remote access that is provided by Apache. That would be a very nice addition that supports the community to become more secure.

Apache Guacamole: https://guacamole.apache.org/

# Design

Guacamole consist of a guacamole webserver that connects with a guacd service using the Guacamole protocol. The guacd service makes the real connections with the remote hosts. At this moment the first design idea is to put the hardware-based device between the Guacamole webserver and the guacd service.

![design](https://raw.githubusercontent.com/macsnoeren/guacamole-datadiode/refs/heads/main/documentation/images/gucamole_data_diode_design.png)

That means that the Guacd is part of the critical infrastructure and the interface to this service is protected with the hardware-based device. Attackers that gets control over the Guacamole webserver shall not be able to lateral move over the hardware-based device to the guacd. The hardware-based device only transfer application data and not network data.

