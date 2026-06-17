#include "../../include/parser/opcode_parser.h"
#include <cstring>
#include <string.h>

// TODO: parse connection requests
ParserState OpcodeParser::Parse(const char *data, size_t len) {
    // Ranges recorded here belong to this call's buffer only.
    denied_ranges.clear();

    // `denying`: inside a disallowed instruction whose bytes are being skipped
    // until its ';', then recorded for excision. 
    bool denying = false;
    
    // `opcode_in_this_call`: the instruction currently being parsed started in
    // this call, so opcode_start_idx is a valid index into `data` (a 
    // prerequisite for cutting it out).
    bool opcode_in_this_call = false;

    // Each byte drives exactly one state. The diagram's two transient states are
    // folded into the state that consumes their trigger byte: READING_DOT into
    // the '.' branch of READING_LENGTH, and HANDLING_ELEMENT into EXPECT_DELIM.
    for (size_t i = 0; i < len; ++i) {
        current_index = i;
        char c = data[i];

        switch (state) {
        case ParserState::READING_LENGTH:
            // If character is a digit
            if (c >= '0' && c <= '9') {
                if (current_length == -1) {
                    current_length = 0;
                    // First digit of a new element. If it is the opcode, record
                    // where this instruction starts in the buffer.
                    if (reading_opcode) {
                        opcode_start_idx = i;
                        opcode_in_this_call = true;
                    }
                }
                // add the digit to the current length (as an integer!)
                current_length = current_length * 10 + (c - '0');

                // Observed length exceeds the buffer. By default that is an
                // untrustworthy stream (the guard); a parser that tolerates large
                // elements keeps framing and just buffers the first
                // MAX_ELEMENT_SIZE bytes for the hooks.
                if (current_length > MAX_ELEMENT_SIZE &&
                    !ToleratesOversizedElements()) {
                    state = ParserState::STREAM_CORRUPTED;
                    return state;
                }
            } else if (c == '.') {
                // The '.' closes the length: decide the next branch from it
                // (folds in the diagram's READING_DOT).
                if (current_length == -1) {
                    // No length calculated before the dot
                    state = ParserState::STREAM_CORRUPTED;
                    return state;
                }
                current_read = 0;
                if (current_length == 0)
                    state = ParserState::EXPECT_DELIM;
                else
                    state = ParserState::READING_DATA;
            } else {
                // This is not a digit or length-terminating character
                state = ParserState::STREAM_CORRUPTED;
                return state;
            }
            break;

        case ParserState::READING_DATA:
            // Buffer only the first MAX_ELEMENT_SIZE bytes; the surplus of a
            // tolerated oversized element is framed but not stored (and not
            // ASCII-checked, since the hooks never see it).
            if (current_read < MAX_ELEMENT_SIZE) {
                // byte MUST be ascii value
                if (static_cast<unsigned char>(c) > 127) {
                    state = ParserState::STREAM_CORRUPTED;
                    return state;
                }
                element_buffer[current_read] = c;
            }
            ++current_read;

            // current_read has advanced to the observed length
            if (current_read == current_length)
                state = ParserState::EXPECT_DELIM;
            break;

        case ParserState::EXPECT_DELIM: {
            // c must be the ',' or ';' that closes the element now sitting in
            // element_buffer (folds in the diagram's HANDLING_ELEMENT). While
            // `denying`, validation is skipped — we only parse far enough to
            // find the instruction's ';' so its whole range can be recorded.
            if (c != ',' && c != ';') {
                // This is not a valid delimiter
                state = ParserState::STREAM_CORRUPTED;
                return state;
            }

            if (!denying) {
                // For a tolerated oversized element only the first
                // MAX_ELEMENT_SIZE bytes were buffered, so the hook sees a
                // clamped view.
                uint32_t elem_len =
                    current_length > MAX_ELEMENT_SIZE
                        ? static_cast<uint32_t>(MAX_ELEMENT_SIZE)
                        : static_cast<uint32_t>(current_length);
                GuacElement elem{elem_len, element_buffer};
                bool allowed = reading_opcode ? OnInstructionBegin(elem)
                                              : OnArgument(elem);
                if (!allowed) {
                    // Deny the whole instruction — but only if it started in
                    // this call. If its head arrived in an earlier datagram it
                    // was already forwarded, so we cannot recover and the stream
                    // is corrupted.
                    if (!opcode_in_this_call) {
                        state = ParserState::STREAM_CORRUPTED;
                        return state;
                    }
                    denying = true;
                    reading_opcode = false; // skip the remainder as data
                } else if (reading_opcode) {
                    reading_opcode = false;
                }
            }

            if (c == ';') {
                // End of instruction.
                if (denying) {
                    // Record its full byte range so the caller can excise it.
                    // Parse() never mutates data.
                    denied_ranges.emplace_back(opcode_start_idx,
                                               i - opcode_start_idx + 1);
                    denying = false;
                } else {
                    OnInstructionEnd();
                }
                reading_opcode = true;
            }
            // Comma or semicolon: another element/instruction follows
            current_length = -1;
            state = ParserState::READING_LENGTH;
            break;
        }

        case ParserState::STREAM_CORRUPTED:
        case ParserState::DENIED_DATA:
            // Terminal for this call; nothing more is parsed.
            return state;
        }
    }

    // A denied instruction that never reached its ';' cannot be cut cleanly
    // across datagrams (its head would already be forwarded): corrupt the stream.
    if (denying) {
        state = ParserState::STREAM_CORRUPTED;
        return state;
    }

    // current_length, current_read and element_buffer persist across calls, so a
    // Parse() that ends mid-element resumes correctly. A clean boundary rests at
    // READING_LENGTH with current_length == -1. Report DENIED_DATA when content
    // was denied but the rest parsed cleanly, so the caller calls Excise(); the
    // member `state` still holds the true resting state for the next call.
    if (!denied_ranges.empty())
        return ParserState::DENIED_DATA;
    return state;
}

void OpcodeParser::Excise(char *data, size_t &len) {
    if (denied_ranges.empty())
        return;

    // Single compaction pass: copy the kept bytes between the denied ranges
    // (which are ascending and non-overlapping) down over the gaps.
    size_t write = 0;
    size_t read = 0;
    for (const auto &range : denied_ranges) {
        size_t keep = range.first - read; // bytes before this denied range
        if (write != read)
            std::memmove(data + write, data + read, keep);
        write += keep;
        read = range.first + range.second; // skip past the denied range
    }
    if (write != read)
        std::memmove(data + write, data + read, len - read);
    len = write + (len - read);

    denied_ranges.clear();
}

bool OpcodeParser::OnInstructionBegin(const GuacElement &opcode) {
    // Neutral framer: allow every opcode. The opcode allowlist is a guard policy
    // and lives in GuardOpcodeParser::OnInstructionBegin.
    (void)opcode;
    return true;
}
