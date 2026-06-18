#include "../../shared/include/network/channeltable.h"
#include "../../shared/include/network/netqueue.h"
#include "../../shared/include/network/guacd_client.h"
#include "../../shared/include/network/reader_group.h"
#include "../../shared/include/network/udpreceiver.h"
#include "../../shared/include/network/udpsender.h"
#include "../../shared/include/util/netargs.h"
#include "../../shared/include/util/queue_monitor.h"
#include "../include/nethandlers/guacd_send_handler.h"
#include "../include/nethandlers/udp_recv_handler.h"
#include "../include/nethandlers/udp_send_handler.h"
#include "../include/running.h"
#include <atomic>
#include <iostream>
#include <optional>
#include <signal.h>
#include <string>
#include <thread>

std::atomic<bool> running = true;

/*
 * @brief Signals all threads to stop when an interrupt signal is received
 */
void interrupt_handler(int signum) {
    std::cout << "Interrupt received, stopping program..." << std::endl;
    running = false;
}

/*
 * @brief Starts the guacd broker that imitates the Guacamole web server and
 * bridges Guacamole traffic.
 *
 * Reacts to channel lifecycle messages from the bridge, opening one guacd
 * connection per channel. Handlers run on separate threads and synchronize
 * messages using thread-safe queues.
 */
int main(int argc, char *argv[]) {
    if (argc != 6) {
        std::cerr << "Usage: " << argv[0] << "\n"
                  << "\t<guacd_ip>: guacd's IP address\n"
                  << "\t<guacd_port>: guacd's listening port\n"
                  << "\t<udp_recv_port>: port where the broker receives traffic from\n"
                  << "\t<udp_send_ip>: address where the broker sends guacd traffic to (lrx_proxy)\n"
                  << "\t<udp_send_port>: port where the broker sends guacd traffic to\n"
                  << "\nExample: " << argv[0]
                  << " 127.0.0.1 4822 5501 10.0.0.2 5601" << std::endl;
        return 1;
    }

    const char *guacd_ip = argv[1];
    const char *udp_send_ip = argv[4];
    std::optional<int> p_guacd = ParsePort(argv[2]);
    std::optional<int> p_recv = ParsePort(argv[3]);
    std::optional<int> p_send = ParsePort(argv[5]);
    if (!p_guacd || !p_recv || !p_send) {
        std::cerr << "Error: ports must be integers in [1, 65535]" << std::endl;
        return 1;
    }
    int guacd_port = p_guacd.value();
    int udp_recv_port = p_recv.value();
    int udp_send_port = p_send.value();

    // Set interrupt handler
    struct sigaction sa{};
    sa.sa_handler = interrupt_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr); // `docker compose down`/`stop` send SIGTERM

    // Initialize UDP and TCP infrastructure
    int exit;
    UDPReceiver udp_receiver(udp_recv_port);
    if ((exit = udp_receiver.Initialize()) != 0)
        return exit;

    std::cout << "Initialized UDP receiver on port " << udp_recv_port
              << std::endl;

    UDPSender udp_sender(udp_send_ip, udp_send_port);
    if ((exit = udp_sender.Initialize()) != 0)
        return exit;

    std::cout << "Initialized UDP sender for " << udp_send_ip << ":"
              << udp_send_port << std::endl;

    auto guacd_client = GuacdClient(guacd_ip, guacd_port);
    ChannelTable table;
    ReaderGroup readers; // Tracks the per-channel guacd reader threads for shutdown
    NetQueue recv_queue;
    NetQueue send_queue;

    // Start the handler threads. The processor gates each channel on approval
    // and dials guacd; the UDP handlers ferry between the bridge and the queues.
    GuacdSendHandler guacd_send_handler;
    UDPSendHandler udp_send_handler;
    UDPRecvHandler udp_recv_handler;

    std::thread t_guacd_send =
        guacd_send_handler.Run(recv_queue, send_queue, guacd_client, table, readers);
    std::thread t_udp_send = udp_send_handler.Run(send_queue, udp_sender);
    std::thread t_udp_recv = udp_recv_handler.Run(recv_queue, udp_receiver);

    // Optional diagnostic (set QUEUE_STATS_MS): watch for the return-path
    // send_queue growing, which means the bridge can't drain guacd's output.
    // std::thread t_qstats =
    //     StartQueueMonitor(recv_queue, send_queue, running, "gcdbroker");

    // Shutdown ordering (SIGINT clears `running`): the UDP receiver's blocked
    // recvfrom times out (SO_RCVTIMEO), so t_udp_recv falls out of its loop first
    // and stops feeding recv_queue. Closing recv_queue drains t_guacd_send, which
    // both spawns the readers and is the last producer for send_queue; once it
    // has joined, only the detached guacd readers still touch the table. We wake
    // those readers (they block in recv()) by shutting down their fds — each
    // reader still owns its own close() — and WaitAll() for them before
    // destroying the state they capture. Finally send_queue is closed to drain
    // t_udp_send.
    // if (t_qstats.joinable())
    //     t_qstats.join();
    t_udp_recv.join();
    recv_queue.Close();
    t_guacd_send.join();

    for (int fd : table.Fds())
        guacd_client.Shutdown(fd);
    readers.WaitAll();

    send_queue.Close();
    t_udp_send.join();
}
