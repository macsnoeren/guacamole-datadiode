#include "../include/guacparser.h"
#include <cstring>
#include <iostream>
#include <string.h>
#include <string>

ParserState GuacParser::Parse(const char *data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        char c = data[i];

        switch (phase) {
        case ParserPhase::READING_LENGTH:
            // If character is a digit
            if (c >= '0' && c <= '9') {
                if (current_length == -1)
                    current_length = 0;
                // add the digit to the current length (as an integer!)
                current_length = current_length * 10 + (c - '0');

                // Observed length exceeds the maximum allowed
                if (current_length > MAX_ELEMENT_SIZE) {
                    state = ParserState::INVALID;
                    return state;
                }
            } else if (c == '.') {
                // No length calculated after finding the dot
                if (current_length == -1) {
                    state = ParserState::INVALID;
                    return state;
                }

                current_read = 0;
                if (current_length == 0)
                    phase = ParserPhase::EXPECT_DELIM;
                else if (next_args_max_length == 0 || static_cast<uint32_t>(current_length) <= next_args_max_length)
                    phase = ParserPhase::READING_DATA;
                else {
                    // Argument length exceeded next_args_max_length
                    state = ParserState::INVALID;
                    return state;
                }
            } else {
                // This is not a digit or length-terminating character
                state = ParserState::INVALID;
                return state;
            }
            break;
        case ParserPhase::READING_DATA:
            // byte MUST be ascii value
            if (static_cast<unsigned char>(c) > 127) {
                state = ParserState::INVALID;
                return state;
            }

            // add data to the buffer, then advance current_read
            element_buffer[current_read++] = c;

            // current_read has advanced to the observed length
            if (current_read == static_cast<uint32_t>(current_length)) {
                phase = ParserPhase::EXPECT_DELIM;
            }
            break;
        case ParserPhase::EXPECT_DELIM: { // Scope the GuacElement struct for
                                          // automatic destruction
            if (current_length < 0) {
                state = ParserState::INVALID;
                return state;
            }
            GuacElement elem{static_cast<uint32_t>(current_length),
                             element_buffer};

            if (c == ',' || c == ';') {
                if (reading_opcode) {
                    // If this was an opcode, check if it is allowed
                    // Also sets next_args_max_length if needed
                    if (!OnInstructionBegin(elem)) {
                        std::cout << "Denied opcode "
                                  << std::string(elem.ptr, elem.len)
                                  << std::endl;
                        state = ParserState::DENIED_OPCODE;
                        return state;
                    }

                    reading_opcode = false;
                } else {
                    // If this was an argument, check if it was allowed
                    if (!OnArgument(elem)) {
                        state = ParserState::INVALID;
                        return state;
                    }
                }
            }

            if (c == ',') {
                // Comma read, expecting another argument
                current_length = -1;
                phase = ParserPhase::READING_LENGTH;
            } else if (c == ';') {
                // Semicolon read, expecting end of buffer or another opcode
                OnInstructionEnd();
                reading_opcode = true;
                next_args_max_length = 0;
                current_length = -1;
                phase = ParserPhase::READING_LENGTH;
            } else {
                // This is not a valid delimiter
                state = ParserState::INVALID;
                return state;
            }
            break;
        }
        }
    }

    // current_length will be nonzero if it is expecting data but the loop has
    // ended. Since current_length and phase are stored outside the loop, the
    // parser can continue when receiving new data. element_buffer is also
    // stored outside the loop, so it can continue parsing, since the length is
    // still preserved. if current_length is minus one, that means all opcodes
    // so far have been parsed.
    state = (phase == ParserPhase::READING_LENGTH && current_length == -1)
                ? ParserState::READY
                : ParserState::PARSING;
    return state;
}

bool GuacParser::OnInstructionBegin(const GuacElement &opcode) {
    /*
     * 3.key
     * 3.ack
     * 3.end
     * 4.size
     * 4.name
     * 4.argv
     * 4.sync
     * 5.audio
     * 5.video
     * 5.image
     * 5.mouse
     * 6.select
     * 7.connect
     * 8.timezone
     * 10.disconnect
     */
    switch (opcode.len) {
    case 3:
        return !memcmp(opcode.ptr, "key", 3) ||
               !memcmp(opcode.ptr, "ack", 3) ||
               !memcmp(opcode.ptr, "nop", 3) ||
               !memcmp(opcode.ptr, "end", 3);
    case 4:
        if (!memcmp(opcode.ptr, "blob", 4)) {
            next_args_max_length = CLIPBOARD_MAX_BYTES;
            return true;
        }
        return !memcmp(opcode.ptr, "size", 4) ||
               !memcmp(opcode.ptr, "name", 4) ||
               !memcmp(opcode.ptr, "argv", 4) || !memcmp(opcode.ptr, "sync", 4);
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
