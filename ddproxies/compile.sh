#!/bin/sh
g++ -o guac-proxy-ingress guac-proxy-ingress.cpp
#g++ -o guac-proxy-egress guac-proxy-egress.cpp
g++ -o guac-proxy-egress guac-proxy-egress.cpp include/tcpserver.hpp -I.
