#pragma once

#include <string>

/**
 * @brief An approval verdict with an optional human-readable reason
 */
struct ApprovalResult {
    bool approved;
    std::string reason; // populated on denial, empty on approval
};

/**
 * @brief Decides whether a forged Guacamole connection may reach guacd.
 *
 * The forwarded `connect` instruction doubles as the approval request. This is
 * the OT-side gate: a PoC stand-in for a human operator at htx_proxy. It
 * approves by default; setting the GCD_APPROVE environment variable to "deny"
 * makes it deny every request (used to exercise the denied path end-to-end).
 */
class Approver {
  public:
    /**
     * @brief Decides on a connection request.
     * @param protocol: the protocol named in `select` (e.g. "ssh")
     * @param connect: the captured handshake (for context / future prompting)
     * @return The approval result
     */
    ApprovalResult HandleRequest(const std::string &protocol,
                                 const std::string &connect);
};
