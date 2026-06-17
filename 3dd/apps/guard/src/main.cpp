#include "../../shared/include/network/multiplexer.h"
#include "../../shared/include/network/udpreceiver.h"
#include "../../shared/include/network/udpsender.h"
#include "../../shared/include/parser/opcode_parser.h"
#include "../include/guard_opcode_parser.h"
#include "../../shared/include/util/control_channel.h"
#include "../../shared/include/util/netargs.h"
#include "../include/approver.h"
#include <atomic>
#include <csignal>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

// Cleared by the SIGINT handler; the receive loop polls it to stop. The guard's
// UDPReceiver has a recv timeout, so the loop wakes to observe this even when no
// traffic is arriving.
std::atomic<bool> running = true;

/*
 * @brief Clears the run flag on SIGINT.
 *
 * Deliberately does no I/O: it must stay async-signal-safe. (Logging on
 * shutdown is left to the future logging facility.)
 */
void interrupt_handler(int) { running = false; }

/*
 * @brief Human-readable name for a parser state, for logging
 */
const char *state_name(ParserState state) {
    switch (state) {
    case ParserState::READING_LENGTH:
        return "READING_LENGTH";
    case ParserState::READING_DATA:
        return "READING_DATA";
    case ParserState::EXPECT_DELIM:
        return "EXPECT_DELIM";
    case ParserState::DENIED_DATA:
        return "DENIED_DATA";
    case ParserState::STREAM_CORRUPTED:
    default:
        return "STREAM_CORRUPTED";
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

    std::optional<int> src_port = ParsePort(argv[1]);
    const char *dst_ip = argv[2];
    std::optional<int> dst_port = ParsePort(argv[3]);
    if (!src_port || !dst_port) {
        std::cerr << "Error: <src_port> and <dst_port> must be integers in "
                     "[1, 65535]"
                  << std::endl;
        return 1;
    }

    // Stop the receive loop cleanly on SIGINT.
    struct sigaction sa{};
    sa.sa_handler = interrupt_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);

    int rc;
    UDPReceiver receiver = UDPReceiver(src_port.value());
    if ((rc = receiver.Initialize()) != 0)
        return rc;

    std::cout << "Listening on UDP port " << src_port.value() << std::endl;

    UDPSender sender = UDPSender(dst_ip, dst_port.value());
    if ((rc = sender.Initialize()) != 0)
        return rc;

    // One parser per channel; channels that violate policy are poisoned
    std::unordered_map<uint16_t, GuardOpcodeParser> parsers;
    std::unordered_set<uint16_t> poisoned;

    // The guard is the approval gate: the operator decides on each inert CREATE
    // request here. `approved` holds the channels cleared to carry Guacamole.
    Approver approver;
    std::unordered_set<uint16_t> approved;

    // Set by the control listener on a global "deny"; the main loop observes it
    // and tears down every still-approved channel, so a deny disconnects live
    // sessions and not just future requests.
    std::atomic<bool> deny_teardown{false};

    // Out-of-band control listener: a plaintext "approve"/"deny" datagram on the
    // control port flips the global approval switch at runtime (relayed here by
    // gmlbroker). Its UDPReceiver has the same 200 ms recv timeout, so the loop
    // observes `running` and stops on SIGINT.
    UDPReceiver control_receiver(ControlChannel::APPROVAL_CONTROL_PORT);
    if ((rc = control_receiver.Initialize()) != 0)
        return rc;
    std::cout << "Listening for approval toggles on UDP port "
              << ControlChannel::APPROVAL_CONTROL_PORT << std::endl;

    std::thread control_thread([&approver, &control_receiver, &deny_teardown]() {
        char buf[256];
        while (running) {
            int n = control_receiver.Receive(buf, sizeof(buf));
            if (n <= 0)
                continue;
            std::optional<bool> mode =
                ControlChannel::ParseApprovalToggle(std::string(buf, n));
            if (!mode) {
                std::cerr << "guard: ignored unrecognised approval command"
                          << std::endl;
                continue;
            }
            approver.SetApprove(*mode);
            // A deny also disconnects channels already carrying Guacamole; the
            // main loop owns the channel state, so just flag it here.
            if (!*mode)
                deny_teardown.store(true, std::memory_order_relaxed);
            std::cout << "guard: approval switch set to "
                      << (*mode ? "APPROVE" : "DENY") << std::endl;
        }
    });

    char buffer[Multiplexer::MAX_DATAGRAM_SIZE + 1];

    while (running) {
        // Act on a global deny: tear down every still-approved channel. This
        // operation checks if `deny_teardown` is true, and sets it to false.
        if (deny_teardown.exchange(false, std::memory_order_relaxed)) {
            for (uint16_t ch : approved) {
                BridgeMessage shutdown{ch, ChannelAction::SHUTDOWN_CHANNEL, ""};
                std::string wire = Multiplexer::Serialize(shutdown);
                sender.Send(wire.data(), wire.size());
                parsers.erase(ch);
                std::cout << "guard: channel " << (int)ch
                          << " torn down by global deny" << std::endl;
            }
            approved.clear();
        }

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
        // CREATE is the inert approval request: the operator decides here.
        case ChannelAction::CREATE_CHANNEL: {
            // Fresh state for a (possibly reused) channel id
            parsers[msg.channel] = GuardOpcodeParser{};
            poisoned.erase(msg.channel);
            approved.erase(msg.channel);

            // The CREATE payload is the inert request id (never Guacamole).
            const std::string &request_id = msg.payload;
            ApprovalResult verdict = approver.HandleRequest(request_id);

            // Forward the CREATE downstream (gcdbroker dials guacd only when it
            // sees the APPROVAL verdict, never on CREATE alone).
            sender.Send(buffer, received);

            // Emit the verdict forward; gcdbroker flips it onto the return path
            // back to gmlbroker (the guard is forward-only). Payload byte 0 is
            // the printable verdict char, the rest is the request id.
            char v = verdict.approved ? APPROVAL_APPROVE : APPROVAL_DENY;
            BridgeMessage approval{msg.channel, ChannelAction::APPROVAL,
                                   std::string(1, v) + request_id};
            std::string wire = Multiplexer::Serialize(approval);
            sender.Send(wire.data(), wire.size());

            if (verdict.approved) {
                approved.insert(msg.channel);
                std::cout << "guard: channel " << (int)msg.channel
                          << " APPROVED" << std::endl;
            } else {
                // Denied: no Guacamole will ever cross; tear the channel down.
                BridgeMessage shutdown{msg.channel,
                                       ChannelAction::SHUTDOWN_CHANNEL, ""};
                std::string sd = Multiplexer::Serialize(shutdown);
                sender.Send(sd.data(), sd.size());
                parsers.erase(msg.channel);
                std::cout << "guard: channel " << (int)msg.channel
                          << " DENIED" << std::endl;
            }
            break;
        }

        // Remove channel reference
        case ChannelAction::SHUTDOWN_CHANNEL:
            parsers.erase(msg.channel);
            poisoned.erase(msg.channel);
            approved.erase(msg.channel);
            sender.Send(buffer, received);
            std::cout << "guard: channel " << (int)msg.channel
                      << " SHUTDOWN, forwarded SHUTDOWN"
                      << std::endl;
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

            // No Guacamole crosses the bridge until the channel is approved.
            if (!approved.count(msg.channel)) {
                std::cerr << "guard: channel " << (int)msg.channel
                          << " received traffic but was not approved, dropped "
                          << msg.payload.size() << " bytes" << std::endl;
                break;
            }

            GuardOpcodeParser &parser = parsers[msg.channel];
            ParserState state =
                parser.Parse(msg.payload.data(), msg.payload.size());

            // The stream can no longer be trusted. Tell the OT side to tear the
            // channel down, forget its parser, and drop everything further on
            // it (a fresh CREATE for a reused id will clear the poison).
            if (state == ParserState::STREAM_CORRUPTED) {
                BridgeMessage shutdown{msg.channel,
                                       ChannelAction::SHUTDOWN_CHANNEL, ""};
                std::string wire = Multiplexer::Serialize(shutdown);
                sender.Send(wire.data(), wire.size());

                parsers.erase(msg.channel);
                approved.erase(msg.channel);
                poisoned.insert(msg.channel);
                std::cerr << "guard: channel " << (int)msg.channel
                          << " STREAM_CORRUPTED, sent SHUTDOWN and dropped "
                          << msg.payload.size() << " bytes" << std::endl;
                break;
            }

            // Disallowed opcode(s): excise them from the send buffer and forward
            // the trimmed remainder so the rest of the allowed traffic flows.
            if (state == ParserState::DENIED_DATA) {
                size_t orig = msg.payload.size();
                size_t plen = orig;

                std::cerr << "DENIED_DATA: channel " << (int)msg.channel
                          << " excising " << (orig - plen) << " bytes of"
                             " denied content, got: '" << msg.payload
                          << "'" << std::endl;

                parser.Excise(msg.payload.data(), plen);
                msg.payload.resize(plen);

                if (msg.payload.empty())
                    break; // nothing left to forward

                std::string wire = Multiplexer::Serialize(msg);
                sender.Send(wire.data(), wire.size());
            } else {
                // Clean: forward the datagram verbatim.
                sender.Send(buffer, received);
            }
            break;
        }
        }
    }

    // SIGINT cleared `running`; the control listener's recv times out and the
    // thread leaves its loop, so join it before returning.
    control_thread.join();
}
