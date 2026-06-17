#pragma once

#include "../../shared/include/parser/opcode_parser.h"
#include <cstddef>

/**
 * @brief Swallows the browser's keepalive/no-op opcodes from the forward stream.
 *
 * `sync` and `nop` are pure keepalives: the guard blocks them toward guacd and
 * the brokers handle the timing themselves (gmlbroker fakes a `sync` toward the
 * browser, gcdbroker fakes the client `sync` toward guacd; `nop` needs no fake).
 * So the browser's own `sync`/`nop` are redundant and should never cross the
 * bridge; otherwise the guard excises them on every frame. This filter removes
 * them at the source, leaving every other opcode for the guard to validate.
 *
 * It is a neutral framer apart from those two denials: it tolerates large
 * elements (a forward blob is the guard's call, not gmlbroker's) so it never
 * corrupts the stream on size alone.
 */
class ForwardKeepaliveFilter : public OpcodeParser {
  public:
    /**
     * @brief Removes any complete client `sync`/`nop` from `data` in place,
     *        updating `len`. If the chunk cannot be analysed (e.g. an instruction
     *        split across reads), it is left untouched (fail open) and the guard
     *        still validates it.
     */
    void Filter(char *data, size_t &len);

  protected:
    bool OnInstructionBegin(const GuacElement &instr) override;
    bool ToleratesOversizedElements() override { return true; }
};
