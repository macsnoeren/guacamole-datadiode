#pragma once

#include <cstdint>
#include <string>

/**
 * @brief Signifies what a system needs to do when a BridgeMessage is received
 *
 * The value is pre-shifted into the top two bits of the BridgeMessage flags
 * byte, so it can be written to / masked from the wire without further shifting.
 */
enum class ChannelAction : uint8_t {
    // No action; the message carries payload for an existing channel
    NONE = 0,
    // Create a new channel (didn't exist yet)
    CREATE_CHANNEL = 64,   // 0100'0000 in binary
    // Remove the reference to this channel (forget it)
    SHUTDOWN_CHANNEL = 128, // 1000'0000 in binary
    // Approval verdict for a channel, sent on the return path. Payload byte 0 is
    // a printable verdict char ('A' = approve, 'D' = deny); the rest is an
    // optional reason string. Kept NUL-free so it prints cleanly in logs.
    APPROVAL = 192         // 1100'0000 in binary
};

/**
 * @brief Printable verdict characters carried in an APPROVAL payload's first byte
 */
constexpr char APPROVAL_APPROVE = 'A';
constexpr char APPROVAL_DENY = 'D';

/**
 * @brief Each 'message' or 'packet' sent over the 3DD is wrapped in a BridgeMessage
 *
 * Owns its payload bytes so it can safely be moved across threads through a
 * NetQueue. SHUTDOWN_CHANNEL carries an empty payload by convention;
 * CREATE_CHANNEL carries the inert approval-request id; APPROVAL carries a
 * verdict char followed by that id.
 */
struct BridgeMessage {
    uint16_t channel = 0;                       // Identifier for multiplexing Guacamole connections
    ChannelAction action = ChannelAction::NONE;
    std::string payload;                       // Content of the message (owns its bytes)
};

/**
 * @brief Contains methods for multiplexing and demultiplexing bridge network messages
 *
 * All messages sent over the 3DD must be wrapped in a BridgeMessage, so the users
 * using the connections can be distinguished. This class converts between the
 * on-wire representation and the BridgeMessage struct.
 *
 * Wire format (3-byte header, then payload):
 *
 *   byte 0   byte 1   byte 2            byte 3 .. N
 *  +--------+--------+--------+---------------------------+
 *  |   channel (BE)  | flags  |   payload (0..1200 bytes) |
 *  +--------+--------+--------+---------------------------+
 *                      |
 *                      +-- bits 7-6 = ChannelAction, bits 5-0 = reserved (must be 0)
 *
 * The channel ID is two bytes, big-endian (network order).
 */
class Multiplexer {
  public:
    // Size of the BridgeMessage header that precedes every payload
    static constexpr int HEADER_SIZE = 3;
    // Max size that a single payload over the bridge can ever be
    static constexpr int MAX_PAYLOAD_SIZE = 1200;
    // Max size of a full datagram (header + payload)
    static constexpr int MAX_DATAGRAM_SIZE = HEADER_SIZE + MAX_PAYLOAD_SIZE;

    // Bit masks for the flags byte
    static constexpr uint8_t ACTION_MASK = 0xC0;   // 1100'0000
    static constexpr uint8_t RESERVED_MASK = 0x3F; // 0011'1111

    /**
     * @brief Serializes a BridgeMessage to its on-wire representation
     *
     * The caller is responsible for keeping payloads within MAX_PAYLOAD_SIZE
     * (chunking larger reads beforehand).
     * @return The header followed by the payload bytes
     */
    static std::string Serialize(const BridgeMessage &message);

    /**
     * @brief Takes a raw BridgeMessage buffer and writes it to message
     * @return Whether the buffer was a well-formed BridgeMessage. False when the
     *         buffer is too short, sets reserved bits, has an unknown action, or
     *         carries an oversized payload.
     */
    static bool TryCast(const char *buffer, size_t len, BridgeMessage &message);
};
