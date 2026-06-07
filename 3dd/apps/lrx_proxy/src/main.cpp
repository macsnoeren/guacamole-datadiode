#include "../../shared/include/network/udpreceiver.h"
#include "../../shared/include/network/udpsender.h"
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

/**
 * @brief Relays network traffic between the htx_proxy and ltx_proxy
 *
 * Receives UDP packets from one interface and sends them over another UDP interface
 */
int main(int argc, char *argv[]) {
    if (argc < 4) {
        std::cerr
            << "Usage: " << argv[0] << "\n"
            << "\t<src_port>: port where the proxy receives traffic (from htx_proxy)\n"
            << "\t<dst_ip>: IP address where the proxy sends traffic (to ltx_proxy)\n"
            << "\t<dst_port>: port where the proxy sends traffic to\n"
            << "\tExample: " << argv[0] << " 5005 10.0.0.2 6006"
            << std::endl;
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

    char buffer[65535];

    while (true) {
        int received = receiver.Receive(buffer, sizeof(buffer));

        std::cout << "Sending" << received << " bytes from :" << src_port
                  << " -> " << dst_ip << ":" << dst_port << std::endl;
        sender.Send(buffer, received);
    }
}
