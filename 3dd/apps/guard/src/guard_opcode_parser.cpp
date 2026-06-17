#include "../include/guard_opcode_parser.h"
#include <charconv>
#include <cstring>
#include <system_error>

namespace {

// Parses a Guacamole stream index argument; returns -1 if it is not a clean,
// whole number (no trailing junk).
int parse_stream_index(const GuacElement &arg) {
    int idx = 0;
    auto [ptr, ec] = std::from_chars(arg.ptr, arg.ptr + arg.len, idx);
    if (ec != std::errc() || ptr != arg.ptr + arg.len)
        return -1;
    return idx;
}

} // namespace

bool GuardOpcodeParser::IsAllowedOpcode(const GuacElement &opcode) {
    /*
     * Only connection-setup, input and stream-control opcodes may cross toward
     * guacd. `sync` is deliberately NOT allowed: it is blocked on the inbound
     * path and the brokers fake the keepalive instead.
     *
     * 3.key   3.ack   3.nop   3.end
     * 4.size  4.name  4.argv  4.blob
     * 5.audio 5.video 5.image 5.mouse
     * 6.select 7.connect 8.timezone 9.clipboard 10.disconnect
     */
    switch (opcode.len) {
    case 3:
        return !memcmp(opcode.ptr, "key", 3) || !memcmp(opcode.ptr, "ack", 3) ||
               !memcmp(opcode.ptr, "nop", 3) || !memcmp(opcode.ptr, "end", 3);
    case 4:
        return !memcmp(opcode.ptr, "blob", 4) ||
               !memcmp(opcode.ptr, "size", 4) ||
               !memcmp(opcode.ptr, "name", 4) ||
               !memcmp(opcode.ptr, "argv", 4);
    case 5:
        return !memcmp(opcode.ptr, "audio", 5) ||
               !memcmp(opcode.ptr, "video", 5) ||
               !memcmp(opcode.ptr, "image", 5) ||
               !memcmp(opcode.ptr, "mouse", 5);
    case 6:
        return !memcmp(opcode.ptr, "select", 6);
    case 7:
        return !memcmp(opcode.ptr, "connect", 7);
    case 8:
        return !memcmp(opcode.ptr, "timezone", 8);
    case 9:
        return !memcmp(opcode.ptr, "clipboard", 9);
    case 10:
        return !memcmp(opcode.ptr, "disconnect", 10);
    default:
        return false;
    }
}

bool GuardOpcodeParser::OnInstructionBegin(const GuacElement &instr) {
    current_opcode.assign(instr.ptr, instr.len);

    // Reset per-instruction state here rather than in OnInstructionEnd: the base
    // does not call OnInstructionEnd for a denied instruction, but every
    // instruction passes through OnInstructionBegin exactly once.
    current_arg = 0;
    cap_next_arg = false;

    return IsAllowedOpcode(instr);
}

bool GuardOpcodeParser::OnArgument(const GuacElement &arg) {
    ++current_arg;

    // The payload argument of a clipboard blob: deny it (the base then excises
    // the whole blob) when it exceeds the cap.
    if (cap_next_arg) {
        cap_next_arg = false;
        return arg.len <= MAX_CLIPBOARD_BYTES;
    }

    // The first argument of clipboard/blob/end is the stream index.
    if (current_arg == 1) {
        int sidx = parse_stream_index(arg);
        if (current_opcode == "clipboard") {
            clipboard_sidx = sidx; // start tracking this stream
        } else if (current_opcode == "blob") {
            // Only the open clipboard stream may carry blob payload toward guacd;
            // deny any other blob (e.g. a file upload) so it is excised.
            if (sidx < 0 || sidx != clipboard_sidx)
                return false;
            cap_next_arg = true; // the next argument is the clipboard payload
        } else if (current_opcode == "end" && sidx == clipboard_sidx) {
            clipboard_sidx = -1; // stream closed, stop tracking
        }
    }

    return true;
}
