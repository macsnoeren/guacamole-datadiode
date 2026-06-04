#include <string>
#include "../include/return_filter.h"

std::string ReturnFilter::Feed(const char *data, size_t len) {
    if (piping)
        return std::string(data, len);

    ready_end = 0;
    Parse(data, len); // may flip `piping` via OnInstructionEnd
    if (piping)
        return std::string(data + ready_end, data + len);
    return std::string();
}

bool ReturnFilter::OnInstructionBegin(const GuacElement &instr) {
    current_opcode.assign(instr.ptr, instr.len);
    return true;
}

bool ReturnFilter::OnInstructionEnd() {
    if (!piping && current_opcode == "ready") {
        piping = true;
        ready_end = CurrentIndex() + 1; // first byte after ready's ';'
    }
    return true;
}
