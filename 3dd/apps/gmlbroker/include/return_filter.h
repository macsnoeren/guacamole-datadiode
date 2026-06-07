#pragma once

#include "../../shared/include/parser/opcode_parser.h"
#include <string>

/**
 * @brief Strips guacd's real handshake reply from the return stream.
 *
 * gmlbroker already gave the web server canned `args` and a forged
 * `ready,FAKE-ID`. When the approved connection reaches guacd, guacd replies
 * with its own `args` and `ready,REAL-ID` before any drawing. This filter
 * swallows everything up to and including guacd's `ready;`, then pipes the rest
 * (the drawing) through unchanged — so the web server keeps the FAKE-ID and
 * never sees the duplicate handshake.
 *
 * Once piping, frames are forwarded verbatim without parsing, so large drawing
 * elements (e.g. image blobs) are never measured against the parser's limits.
 */
class ReturnFilter : public OpcodeParser {
  public:
    /**
     * @brief Feed guacd return bytes; returns the bytes to forward to the web
     * server (empty while still swallowing the handshake).
     */
    std::string Feed(const char *data, size_t len);

  protected:
    bool OnInstructionBegin(const GuacElement &instr) override;

    bool OnInstructionEnd() override;

  private:
    std::string current_opcode;
    bool piping = false;  // true once guacd's `ready` has been swallowed
    size_t ready_end = 0; // offset of the drawing tail within the ready frame
};
