#pragma once

#include "../../shared/include/parser/opcode_parser.h"
#include <cstddef>
#include <cstdint>
#include <string>

/*
 * @brief Guard parser that bounds clipboard payloads.
 *
 * Guacamole clipboard data arrives as a `clipboard,<stream>,<mime>` open, then
 * one or more `blob,<stream>,<data>` instructions, then `end,<stream>`. This
 * parser remembers the stream index opened by `clipboard`; when a `blob` carries
 * that same index, its payload argument is limited to MAX_CLIPBOARD_BYTES. An
 * oversized payload is denied — the base parser then excises the whole blob and
 * keeps forwarding the rest of the stream, rather than corrupting the channel.
 * Tracking stops on the stream's `end`. A blob that is not on the open clipboard
 * stream (e.g. a file upload) is denied outright, so only clipboard payload
 * crosses toward guacd.
 */
class GuardOpcodeParser : public OpcodeParser {
  public:
    static constexpr uint32_t MAX_CLIPBOARD_BYTES = 50;

  protected:
    bool OnInstructionBegin(const GuacElement &instr) override;
    bool OnArgument(const GuacElement &arg) override;

  private:
    std::string current_opcode;
    int clipboard_sidx = -1;   // index of the open clipboard stream, or -1 if none
    size_t current_arg = 0;    // 1-based position of the argument being parsed
    bool cap_next_arg = false; // the next argument is a clipboard blob payload
};
