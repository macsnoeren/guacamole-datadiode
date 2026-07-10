#pragma once

#include <cstdint>

/**
 * @brief Shared clipboard-cap constants.
 *
 * The guard caps clipboard payload entering the OT environment at this size and
 * drops anything larger; gmlbroker uses the same cap to know which blobs the
 * guard will drop, so it can fake the `ack` the browser is waiting for (guacd
 * never acks a dropped blob). Keeping the cap here stops the two from drifting.
 */
namespace clipboard {
// Maximum plaintext bytes of clipboard payload allowed inbound.
constexpr uint32_t MAX_INPUT_BYTES = 50;
// Payload arrives base64-encoded (~33% larger), so this is the on-wire cap.
constexpr uint32_t MAX_BYTES = static_cast<uint32_t>(MAX_INPUT_BYTES * 1.33f);
} // namespace clipboard
