#include "../include/forward_keepalive_filter.h"
#include <cstring>

void ForwardKeepaliveFilter::Filter(char *data, size_t &len) {
    ParserState st = Parse(data, len);
    if (st == ParserState::STREAM_CORRUPTED) {
        // Could not analyse this chunk cleanly (e.g. a keepalive split across
        // reads). Leave it for the guard and resync the FSM at the next read.
        Reset();
        return;
    }
    // Drop the client keepalive(s) the analysis recorded; a no-op otherwise.
    Excise(data, len);
}

bool ForwardKeepaliveFilter::OnInstructionBegin(const GuacElement &instr) {
    // Deny the broker-handled keepalives `sync` and `nop` (so the base records
    // them for excision); everything else is forwarded for the guard to validate.
    bool is_sync = instr.len == 4 && memcmp(instr.ptr, "sync", 4) == 0;
    bool is_nop = instr.len == 3 && memcmp(instr.ptr, "nop", 3) == 0;
    return !(is_sync || is_nop);
}
