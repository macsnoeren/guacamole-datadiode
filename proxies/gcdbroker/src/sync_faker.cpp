#include "../include/sync_faker.h"
#include <cstring>

std::string SyncFaker::Feed(const char *data, size_t len) {
    echoes.clear();
    // guacd's output is not ours to police, and it can contain bytes the base
    // FSM rejects (non-ASCII text, or UTF-8 whose code-point length differs from
    // its byte length). A latched STREAM_CORRUPTED would silently stop all future
    // sync echoes and make guacd time the user out, so fail open: skip the
    // offending byte, resync, and keep scanning for syncs.
    size_t off = 0;
    while (off < len) {
        if (Parse(data + off, len - off) != ParserState::STREAM_CORRUPTED)
            break;
        off += CurrentIndex() + 1; // past the byte that tripped the parser
        Reset();
    }
    return echoes;
}

bool SyncFaker::OnInstructionBegin(const GuacElement &instr) {
    is_sync = (instr.len == 4 && memcmp(instr.ptr, "sync", 4) == 0);
    arg_index = 0;
    timestamp.clear();
    return true; // never deny: this is a neutral observer of guacd's output
}

bool SyncFaker::OnArgument(const GuacElement &arg) {
    ++arg_index;
    // A sync carries a single argument: its timestamp.
    if (is_sync && arg_index == 1)
        timestamp.assign(arg.ptr, arg.len);
    return true;
}

bool SyncFaker::OnInstructionEnd() {
    // Echo guacd's own timestamp straight back — that is the client's sync ack.
    if (is_sync && !timestamp.empty())
        echoes += "4.sync," + std::to_string(timestamp.size()) + "." +
                  timestamp + ";";
    return true;
}
