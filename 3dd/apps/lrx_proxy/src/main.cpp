#include "../../shared/include/network/udpreceiver.h"
#include "../../shared/include/network/udpsender.h"
#include <arpa/inet.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

// Per-datagram relay logging is off by default (it floods stdout on a busy
// bridge). Set RELAY_VERBOSE=1 to enable it.
static bool relay_verbose() {
    const char *v = std::getenv("RELAY_VERBOSE");
    return v && std::string(v) != "0" && std::string(v) != "false";
}

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

    const bool verbose = relay_verbose();

    while (true) {
        int received = receiver.Receive(buffer, sizeof(buffer));
        if (received > 0) {
            if (verbose)
                std::cout << "Relaying " << received << " bytes (:" << src_port
                          << " -> " << dst_ip << ":" << dst_port << ")"
                          << std::endl;
            sender.Send(buffer, received);
        }
    }
}
