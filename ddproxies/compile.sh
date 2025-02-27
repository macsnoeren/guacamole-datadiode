#!/bin/sh
g++ -std=c++17 -o guac-proxy-ingress guac-proxy-ingress.cpp -I.
g++ -std=c++17 -o guac-proxy-egress guac-proxy-egress.cpp -I.
