#pragma once

#include <cctype>
#include <optional>
#include <string>

/**
 * @brief namespace containing functions for handling approval requests.
 *
 * This functionality is temporary; it will be replaced by a more robust
 * system in the future.
 */
namespace ControlChannel {
    /**
     * @brief control channel for the runtime approval switch.
     *
     * The approval policy is a global, runtime-toggleable approve/deny switch.
     * The operator commands the guard directly from the OT side: the approver
     * console (apps/approver, co-located with the guard) sends a plaintext
     * "approve"/"deny" datagram to this port and the guard applies it. Nothing
     * on the IT side can reach it, so IT cannot influence the gate.
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
}
