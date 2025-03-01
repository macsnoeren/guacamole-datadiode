#!/bin/sh
g++ -std=c++17 -o gmserver gmserver.cpp -I.
g++ -std=c++17 -o gmclient gmclient.cpp -I.
g++ -std=c++17 -o gmproxyin gmproxyin.cpp -I.
g++ -std=c++17 -o gmproxyout gmproxyout.cpp -I.

