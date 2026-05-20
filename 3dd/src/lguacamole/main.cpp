#include "../../include/network/udpsender.h"
#include <iostream>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <ip> <port> <message>\n"
                  << "Example: " << argv[0]
                  << " 127.0.0.1 999 \"hello world\"\n";
        return 1;
    }

    const char *ip = argv[1];
    int port = std::stoi(argv[2]);
    std::string message = argv[3];

    UDPSender sender = UDPSender(ip, port);
    sender.Initialize();
    sender.Send(message.c_str(), message.size());
}
