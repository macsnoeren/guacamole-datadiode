#include "../../include/network/multiplexer.h"

std::string Multiplexer::Serialize(const BridgeMessage &message) {
    // Copy message into the out string
    std::string out;
    out.reserve(HEADER_SIZE + message.payload.size());
    out.push_back(static_cast<char>(message.channel));
    out.push_back(static_cast<char>(static_cast<uint8_t>(message.action)));
    out.append(message.payload);
    return out;
}

bool Multiplexer::TryCast(const char *buffer, size_t len, BridgeMessage &message) {
    // Buffer is null or not large enough
    if (buffer == nullptr || len < static_cast<size_t>(HEADER_SIZE))
        return false;

    uint8_t channel = static_cast<uint8_t>(buffer[0]);
    uint8_t flags = static_cast<uint8_t>(buffer[1]);

    // Lower 6 bits are reserved and must be zero
    if (flags & RESERVED_MASK)
        return false;

    // Set the channel action
    ChannelAction action;
    switch (flags & ACTION_MASK) {
    case static_cast<uint8_t>(ChannelAction::NONE):
        action = ChannelAction::NONE;
        break;
    case static_cast<uint8_t>(ChannelAction::CREATE_CHANNEL):
        action = ChannelAction::CREATE_CHANNEL;
        break;
    case static_cast<uint8_t>(ChannelAction::SHUTDOWN_CHANNEL):
        action = ChannelAction::SHUTDOWN_CHANNEL;
        break;
    case static_cast<uint8_t>(ChannelAction::APPROVAL):
        action = ChannelAction::APPROVAL;
        break;
    default:
        return false;
    }

    // Payload too large
    size_t payload_len = len - HEADER_SIZE;
    if (payload_len > static_cast<size_t>(MAX_PAYLOAD_SIZE))
        return false;

    message.channel = channel;
    message.action = action;
    message.payload.assign(buffer + HEADER_SIZE, payload_len);
    return true;
}
