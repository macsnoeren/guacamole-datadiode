#pragma once

#include <cctype>
#include <optional>
#include <string>

/**
 * @brief control channel for the runtime approval switch.
 *
 * The approval policy is a global, runtime-toggleable approve/deny switch.
 * nettest sends a plaintext "approve"/"deny" datagram to gmlbroker's control port;
 * gmlbroker relays it forward to the guard's control port and the guard applies
 * it. The port is the same on both nodes.
 */
constexpr int APPROVAL_CONTROL_PORT = 4999;

/**
 * @brief Parses a plaintext approval-toggle command.
 *
 * Accepts "approve"/"deny" case-insensitively, ignoring surrounding whitespace.
 *
 * @return The desired approve flag (true = approve, false = deny), or
 *         std::nullopt for anything unrecognised.
 */
inline std::optional<bool> ParseApprovalToggle(const std::string &raw) {
    std::string s;
    for (char c : raw) {
        if (!std::isspace(static_cast<unsigned char>(c)))
            s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (s == "approve")
        return true;
    if (s == "deny")
        return false;
    return std::nullopt;
}
