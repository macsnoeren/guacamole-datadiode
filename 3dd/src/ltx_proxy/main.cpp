#include "../../include/network/udpreceiver.h"
#include "../../include/network/udpsender.h"
#include <arpa/inet.h>
#include <cstring>
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
    const int buf_len = 4096;

    UDPReceiver receiver = UDPReceiver(src_port);
    receiver.Initialize();

    UDPSender sender = UDPSender(dst_ip, dst_port);
    sender.Initialize();

    while (true) {
        char buffer[buf_len];
        std::memset(&buffer, 0, buf_len);
        int received = receiver.Receive(buffer, buf_len);

        std::cout << "Sending " << received << " bytes from :" << src_port
                  << " -> " << dst_ip << ":" << dst_port << "\n";
        sender.Send(buffer, received);
    }
}
