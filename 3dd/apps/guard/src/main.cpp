#include "../../shared/include/network/udpreceiver.h"
#include "../../shared/include/network/udpsender.h"
#include "../include/guacparser.h"
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <src_port> <dst_ip> <dst_port>\n"
                  << "Example: " << argv[0] << " 5005 10.0.0.2 6006 \n";
        return 1;
    }

    int src_port = std::stoi(argv[1]);
    const char *dst_ip = argv[2];
    int dst_port = std::stoi(argv[3]);

    UDPReceiver receiver = UDPReceiver(src_port);
    receiver.Initialize();

    std::cout << "Listening on UDP port " << src_port << std::endl;

    UDPSender sender = UDPSender(dst_ip, dst_port);
    sender.Initialize();

    auto parser = GuacParser();
    char buffer[65535];

    while (true) {
        int received = receiver.Receive(buffer, sizeof(buffer));
        parser.Parse(buffer, sizeof(buffer));

        std::cout << "Sending " << received << " bytes from :" << src_port
                  << " to " << dst_ip << ":" << dst_port << std::endl;
        sender.Send(buffer, received);
    }
}
