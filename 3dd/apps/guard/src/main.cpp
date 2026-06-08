#include "../../shared/include/network/multiplexer.h"
#include "../../shared/include/network/udpreceiver.h"
#include "../../shared/include/network/udpsender.h"
#include "../../shared/include/parser/opcode_parser.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>

/*
 * @brief Human-readable name for a parser state, for logging
 */
const char *state_name(ParserState state) {
    switch (state) {
    case ParserState::READY:
        return "READY";
    case ParserState::PARSING:
        return "PARSING";
    case ParserState::DENIED_OPCODE:
        return "DENIED_OPCODE";
    case ParserState::INVALID:
    default:
        return "INVALID";
    }
}

/*
 * @brief Receives multiplexed UDP traffic, validates the Guacamole payload of
 * each channel, and forwards only valid traffic.
 *
 * Channel lifecycle messages (CREATE/SHUTDOWN) are passed through untouched; the
 * guard only inspects NONE payloads. Because channels are multiplexed over a
 * single UDP stream, each channel keeps its own Finite State Machine parser.
 * Disallowed opcodes and traffic that is not valid Guacamole are blocked, and a
 * channel that violates policy is poisoned until it is torn down.
 */
int main(int argc, char *argv[]) {
    if (argc < 4) {
        std::cerr
            << "Usage: " << argv[0] << "\n"
            << "\t<src_port>: port where the guard receives traffic (from "
               "ltx_proxy)\n"
            << "\t<dst_ip>: IP address where the guard sends validated traffic "
               "(to hrx_proxy)\n"
            << "\t<dst_port>: port where the guard sends validated traffic to\n"
            << "\tExample: " << argv[0] << " 5005 10.0.0.2 6006 \n";
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

    // One parser per channel; channels that violate policy are poisoned
    std::unordered_map<uint8_t, OpcodeParser> parsers;
    std::unordered_set<uint8_t> poisoned;

    char buffer[Multiplexer::MAX_DATAGRAM_SIZE + 1];

    while (true) {
        int received = receiver.Receive(buffer, sizeof(buffer));
        if (received <= 0)
            continue;

        // Cannot read this datagram, its invalid
        BridgeMessage msg;
        if (!Multiplexer::TryCast(buffer, received, msg)) {
            std::cerr << "guard: dropped malformed datagram (" << received
                      << " bytes)" << std::endl;
            continue;
        }

        switch (msg.action) {
        // Create a new parser
        case ChannelAction::CREATE_CHANNEL:
            // Fresh parser for a (possibly reused) channel id
            parsers[msg.channel] = OpcodeParser{};
            poisoned.erase(msg.channel);
            sender.Send(buffer, received);
            std::cout << "guard: channel " << (int)msg.channel
                      << " CREATE, forwarded" << std::endl;
            break;

        // Remove channel reference
        case ChannelAction::SHUTDOWN_CHANNEL:
            parsers.erase(msg.channel);
            poisoned.erase(msg.channel);
            sender.Send(buffer, received);
            std::cout << "guard: channel " << (int)msg.channel
                      << " SHUTDOWN, forwarded" << std::endl;
            break;

        case ChannelAction::NONE:
        default: {
            // Keep track of poisoned channels
            if (poisoned.count(msg.channel)) {
                std::cerr << "guard: channel " << (int)msg.channel
                          << " poisoned, dropped " << msg.payload.size()
                          << " bytes" << std::endl;
                break;
            }

            ParserState state =
                parsers[msg.channel].Parse(msg.payload.data(),
                                           msg.payload.size());

            // Invalid traffic, channel is now poisoned and cannot continue
            if (state == ParserState::INVALID ||
                state == ParserState::DENIED_OPCODE) {
                std::cout << "Could not parse payload "
                          << msg.payload
                          << std::endl;
                poisoned.insert(msg.channel);
                std::cerr << "guard: channel " << (int)msg.channel
                          << " dropped " << msg.payload.size() << " bytes ("
                          << state_name(state) << "), channel poisoned"
                          << std::endl;
                break;
            }

            sender.Send(buffer, received);
            std::cout << "guard: channel " << (int)msg.channel << " forwarded "
                      << msg.payload.size() << " bytes (" << state_name(state)
                      << ")" << std::endl;
            break;
        }
        }
    }
}
