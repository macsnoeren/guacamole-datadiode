#pragma once

#include "../../shared/include/parser/opcode_parser.h"
#include <string>

/**
 * @brief Locates the complete `connect` instruction in a replayed handshake.
 *
 * gmlbroker replays the client handshake across the bridge; gcdbroker buffers it
 * until the terminating `connect` arrives, which doubles as the approval
 * request. This is not a security gate (the guard already validated the stream);
 * it only finds that boundary and captures the protocol for the operator prompt.
 */
class ConnectDetector : public OpcodeParser {
  public:
    bool ConnectComplete() const { return connect_complete; }
    const std::string &Protocol() const { return protocol; }

  protected:
    bool OnInstructionBegin(const GuacElement &instr) override {
        current_opcode.assign(instr.ptr, instr.len);
        return true; // boundary-finder, accept every opcode
    }

    bool OnArgument(const GuacElement &arg) override {
        if (current_opcode == "select")
            protocol.assign(arg.ptr, arg.len);
        return true;
    }

    bool OnInstructionEnd() override {
        if (current_opcode == "connect")
            connect_complete = true;
        return true;
    }

  private:
    std::string current_opcode;
    std::string protocol;
    bool connect_complete = false;
};
