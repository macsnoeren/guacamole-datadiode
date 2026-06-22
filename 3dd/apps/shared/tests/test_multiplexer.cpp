#include "../include/network/multiplexer.h"
#include <cassert>
#include <iostream>
#include <string>

/**
 * @brief Asserts that a buffer fails to parse as a BridgeMessage
 */
void test_rejects(const std::string &buffer, const std::string &why) {
    BridgeMessage msg;
    if (Multiplexer::TryCast(buffer.data(), buffer.size(), msg)) {
        std::cerr << "expected TryCast to reject: " << why << std::endl;
        assert(false);
    }
}

/**
 * @brief Asserts that a buffer parses into the expected channel/action/payload
 */
void test_accepts(const std::string &buffer, uint16_t channel,
                  ChannelAction action, const std::string &payload) {
    BridgeMessage msg;
    if (!Multiplexer::TryCast(buffer.data(), buffer.size(), msg)) {
        std::cerr << "expected TryCast to accept buffer of len "
                  << buffer.size() << std::endl;
        assert(false);
    }
    assert(msg.channel == channel);
    assert(msg.action == action);
    assert(msg.payload == payload);
}

/**
 * @brief Builds a raw datagram from a channel ID (big-endian), flags byte and payload
 */
std::string frame(uint16_t channel, uint8_t flags, const std::string &payload) {
    std::string out;
    out.push_back(static_cast<char>(channel >> 8));
    out.push_back(static_cast<char>(channel & 0xFF));
    out.push_back(static_cast<char>(flags));
    out.append(payload);
    return out;
}

/**
 * @brief Serialize then TryCast must reproduce the original message
 */
void test_round_trip() {
    BridgeMessage in;
    in.channel = 42;
    in.action = ChannelAction::NONE;
    in.payload = "5.mouse,3.988,3.369,1.0;";

    std::string wire = Multiplexer::Serialize(in);
    assert(wire.size() == Multiplexer::HEADER_SIZE + in.payload.size());

    BridgeMessage out;
    assert(Multiplexer::TryCast(wire.data(), wire.size(), out));
    assert(out.channel == in.channel);
    assert(out.action == in.action);
    assert(out.payload == in.payload);

    // Control messages with empty payloads
    for (ChannelAction action :
         {ChannelAction::CREATE_CHANNEL, ChannelAction::SHUTDOWN_CHANNEL}) {
        BridgeMessage ctrl;
        ctrl.channel = 7;
        ctrl.action = action;
        std::string ctrl_wire = Multiplexer::Serialize(ctrl);
        assert(ctrl_wire.size() == (size_t)Multiplexer::HEADER_SIZE);

        BridgeMessage parsed;
        assert(
            Multiplexer::TryCast(ctrl_wire.data(), ctrl_wire.size(), parsed));
        assert(parsed.channel == 7);
        assert(parsed.action == action);
        assert(parsed.payload.empty());
    }
}

/**
 * @brief Valid frames parse, including the channel-ID boundaries
 */
void test_valid_frames() {
    test_accepts(frame(0, 0x00, ""), 0, ChannelAction::NONE, "");
    test_accepts(frame(255, 0x00, "data"), 255, ChannelAction::NONE, "data");
    // Channels with the top bit set in either byte must not sign-extend
    test_accepts(frame(128, 0x00, ""), 128, ChannelAction::NONE, "");
    test_accepts(frame(0x8001, 0x00, ""), 0x8001, ChannelAction::NONE, "");
    test_accepts(frame(65535, 0x00, ""), 65535, ChannelAction::NONE, "");
    test_accepts(frame(1, 0x40, ""), 1, ChannelAction::CREATE_CHANNEL, "");
    test_accepts(frame(1, 0x80, ""), 1, ChannelAction::SHUTDOWN_CHANNEL, "");
    test_accepts(frame(1, 0xC0, "A"), 1, ChannelAction::APPROVAL, "A");
    test_accepts(frame(1, 0xC0, "Dno"), 1, ChannelAction::APPROVAL, "Dno");

    // A payload of exactly MAX_PAYLOAD_SIZE is allowed
    std::string max_payload(Multiplexer::MAX_PAYLOAD_SIZE, 'x');
    test_accepts(frame(3, 0x00, max_payload), 3, ChannelAction::NONE,
                 max_payload);
}

/**
 * @brief Malformed frames are rejected
 */
void test_invalid_frames() {
    // Too short to contain a header
    test_rejects("", "empty buffer");
    test_rejects(std::string(1, '\0'), "1-byte buffer");
    test_rejects(std::string(2, '\0'), "2-byte buffer");

    // Reserved bits set in the flags byte
    test_rejects(frame(0, 0x01, ""), "reserved bit 0 set");
    test_rejects(frame(0, 0x3F, ""), "all reserved bits set");
    test_rejects(frame(0, 0x41, "x"), "create with reserved bit set");
    test_rejects(frame(0, 0xC1, "A"), "approval with reserved bit set");

    // Payload one byte over the maximum
    std::string too_big(Multiplexer::MAX_PAYLOAD_SIZE + 1, 'x');
    test_rejects(frame(0, 0x00, too_big), "payload over MAX_PAYLOAD_SIZE");
}

int main() {
    test_round_trip();
    test_valid_frames();
    test_invalid_frames();

    return 0;
}
