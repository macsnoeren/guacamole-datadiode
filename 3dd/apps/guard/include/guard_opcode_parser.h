#pragma once

#include "../../shared/include/parser/opcode_parser.h"
#include <cmath>
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
    static constexpr uint32_t MAX_CLIPBOARD_INPUT_BYTES = 50;

    // The maximum amount of base64 characters that can be sent.
    // Since base64 results in about 33% more characters than
    // their original values, this equation is used.
    static constexpr uint32_t MAX_CLIPBOARD_BYTES = (uint32_t)(MAX_CLIPBOARD_INPUT_BYTES * 1.33f);

  protected:
    bool OnInstructionBegin(const GuacElement &instr) override;
    bool OnArgument(const GuacElement &arg) override;

  private:
    // The guard's opcode allowlist: only these cross toward guacd. (`sync` is
    // deliberately absent — it is blocked inbound and faked by the brokers.)
    static bool IsAllowedOpcode(const GuacElement &opcode);

    std::string current_opcode;
    int clipboard_sidx = -1;   // index of the open clipboard stream, or -1 if none
    size_t current_arg = 0;    // 1-based position of the argument being parsed
    bool cap_next_arg = false; // the next argument is a clipboard blob payload
};
