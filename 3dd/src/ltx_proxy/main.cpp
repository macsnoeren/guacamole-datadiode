#include "../../include/network/udpreceiver.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port>\n"
                  << "Example: " << argv[0] << " 9999\n";
        return 1;
    }

    int port = std::stoi(argv[1]);

    UDPReceiver receiver = UDPReceiver(port);
    receiver.Initialize();

    while (true) {
        char buffer[1024];
        std::memset(&buffer, 0, sizeof(buffer));
        int received = receiver.Receive(buffer, 1024);

        std::cout << "Received " << received << " bytes from " << ":" << port
                  << " -> \"" << buffer << "\"\n";
    }
}
