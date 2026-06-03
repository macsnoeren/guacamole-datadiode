#pragma once

#include "../../shared/include/parser/opcode_parser.h"
#include <string>
#include <vector>

/**
 * @brief States that the forged handshake can be in
 */
enum class HandshakeState {
    UNESTABLISHED,         // before select
    EXCHANGING_PARAMETERS, // args sent, awaiting connect
    ESTABLISHED,           // ready + waiting screen sent
    INVALID_HANDSHAKE      // input could not be parsed
};

/**
 * @brief Forges the guacd side of the Guacamole handshake toward the web server.
 *
 * Subclasses OpcodeParser and drives off the incoming client opcode stream:
 *   select       -> reply with canned `args` (per protocol, pinned to guacd 1.6.0)
 *   size/.../name-> captured (display size, connection params)
 *   connect      -> reply `ready,<FAKE-ID>` + a solid-colour waiting screen
 *
 * It does not contact guacd; the stored protocol and connect values are kept so a
 * later stage can replay the real handshake once a connection has been approved.
 */
class HandshakeForger : public OpcodeParser {
  public:
    HandshakeForger() = default;

    /**
     * @brief Feed bytes received from the web server.
     * @return Bytes to write back to the web server (may be empty).
     */
    std::string Feed(const char *data, size_t len);

    HandshakeState GetHandshakeState() const { return hs_state; }

    /** @brief Protocol named in `select` (e.g. "ssh"); valid after select. */
    const std::string &Protocol() const { return protocol; }

    /** @brief Positional `connect` values; valid once ESTABLISHED. */
    const std::vector<std::string> &ConnectValues() const {
        return connect_values;
    }

    /** @brief Forged connection id sent in `ready`. */
    const std::string &FakeId() const { return fake_id; }

    /**
     * @brief The raw handshake bytes received from the web server, captured
     * verbatim up to and including `connect`. Replayed to the real guacd once a
     * connection is approved.
     */
    const std::string &Handshake() const { return handshake_raw; }

    /**
     * @brief A solid-red "approval denied" overlay followed by a disconnect.
     *
     * Static (no handshake state needed) so the bridge-recv thread can paint it
     * straight onto the web-server socket when a deny verdict arrives.
     */
    static std::string DeniedScreen();

  protected:
    bool OnInstructionBegin(const GuacElement &instr) override;
    bool OnArgument(const GuacElement &arg) override;
    bool OnInstructionEnd() override;

  private:
    HandshakeState hs_state = HandshakeState::UNESTABLISHED;
    std::string current_opcode;          // opcode currently being parsed
    std::string protocol;                // from select
    std::vector<std::string> connect_values; // from connect
    std::vector<std::string> size_args;  // client display size (w, h, dpi)
    std::string fake_id;                 // sent in ready
    std::string handshake_raw;           // verbatim client handshake (for replay)
    std::string out;                     // response accumulator for one Feed

    std::string CannedArgs() const;      // per-protocol args reply
    std::string WaitingScreen() const;   // solid-colour fill (provisional)
};
