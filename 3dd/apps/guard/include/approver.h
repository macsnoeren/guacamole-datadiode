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
 * @brief Decides whether a connection request may proceed toward guacd.
 *
 * The request is an inert, unique identifier minted by gmlbroker — deliberately
 * NOT Guacamole traffic, so the gate never parses attacker-influenced bytes
 * before a human authorizes the connection. This is the OT-side gate, and it
 * lives at the guard: the operator commands the guard directly. It is a PoC
 * stand-in for a human operator. It approves by default; setting the
 * GUARD_APPROVE environment variable to "deny" makes it deny every request
 * (used to exercise the denied path end-to-end).
 */
class Approver {
  public:
    /**
     * @brief Decides on a connection request.
     * @param request_id: the inert unique request identifier
     * @return The approval result
     */
    ApprovalResult HandleRequest(const std::string &request_id);
};
