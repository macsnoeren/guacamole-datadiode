#pragma once

#include <cstdlib>
#include <optional>

/**
 * @brief Parses a TCP/UDP port from a command-line argument.
 *
 * @return The port in [1, 65535], or std::nullopt if the argument is not a whole
 *         number in that range.
 */
inline std::optional<int> ParsePort(const char *arg) {
    char *end = nullptr;
    long value = std::strtol(arg, &end, 10);
    // Reject empty input, trailing junk, and out-of-range values.
    if (end == arg || *end != '\0' || value < 1 || value > 65535)
        return std::nullopt;
    return static_cast<int>(value);
}
