#include "../include/guacparser.h"
#include <cstring>
#include <string.h>

ParserState GuacParser::Parse(const char *data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        char c = data[i];

        switch (phase) {
            case ParserPhase::READING_LENGTH:
                // If character is a digit
                if (c >= '0' && c <= '9') {
                    // add the digit to the current length (as an integer!)
                    current_length = current_length * 10 + (c - '0');

                    // Observed length exceeds the maximum allowed
                    if (current_length > MAX_ELEMENT_SIZE) {
                        state = ParserState::INVALID;
                        return state;
                    }
                } else if (c == '.') {
                    // No length calculated after finding the dot
                    if (current_length == 0) {
                        state = ParserState::INVALID;
                        return state;
                    }

                    current_read = 0;
                    phase = ParserPhase::READING_DATA;
                } else {
                    // This is not a digit or length-terminating character
                    state = ParserState::INVALID;
                    return state;
                }
                break;
            case ParserPhase::READING_DATA:
                // add data to the buffer, then advance current_read
                element_buffer[current_read++] = c;

                // current_read has advanced to the observed length
                if (current_read == current_length) {
                    phase = ParserPhase::EXPECT_DELIM;
                }
                break;
            case ParserPhase::EXPECT_DELIM:
                { // Scope the GuacElement struct for automatic destruction
                    GuacElement elem {
                        current_length,
                        element_buffer
                    };

                    if (reading_opcode) {
                        // If this was an opcode, check if it is allowed
                        if (!OnInstructionBegin(elem)) {
                            state = ParserState::DENIED_OPCODE;
                            return state;
                        }

                        reading_opcode = false;
                    } else {
                        // If this was na argument, check if it was allowed
                        if (!OnArgument(elem)) {
                            state = ParserState::INVALID;
                            return state;
                        }
                    }

                    if (c == ',') {
                        // Comma read, expecting another argument
                        current_length = 0;
                        phase = ParserPhase::READING_LENGTH;
                    } else if (c == ';') {
                        // Semicolon read, expecting end of buffer or another opcode
                        OnInstructionEnd();
                        reading_opcode = true;
                        current_length = 0;
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

    // current_length will be nonzero if it is expecting data but the loop has ended.
    // Since current_length and phase are stored outside the loop,
    // the parser can continue when receiving new data.
    // element_buffer is also stored outside the loop, so it can continue parsing,
    // since the length is still preserved.
    // if current_length is zero, that means all opcodes so far have been parsed.
    state = 
        (phase == ParserPhase::READING_LENGTH &&
         current_length == 0)
        ? ParserState::READY
        : ParserState::PARSING;
    return state;
}

bool GuacParser::OnInstructionBegin(const GuacElement &opcode) {
    // validate opcodes, deny unwanted
    return true;
}

// bool GuacParser::ParseOne(size_t &offset) {
//     GuacInstruction instr{};
//     bool first = true;
//
//     // TODO: break condition
//     while (true) {
//         if (offset >= used)
//             return false;
//
//         // parse length
//         uint32_t len = 0;
//         size_t start = offset;
//
//         // TODO: add a break condition
//         while (offset < used && std::isdigit(buffer[offset])) {
//             // add the next digit to the integer len
//             // The - '0' (0x48) converts the char to a digit.
//             len = len * 10 + (buffer[offset] - '0');
//             offset++;
//         }
//
//         // incomplete
//         if (offset >= used) {
//             offset = start;
//             return false;
//         }
//
//         // no digits in length
//         if (offset == start)
//             return false;
//
//         // no length terminator
//         if (buffer[offset] != '.')
//             return false;
//
//         // at this point, length is a number and terminated correctly
//         offset++;
//
//         // incomplete payload
//         if (offset + len >= used) {
//             offset = start;
//             return false;
//         }
//
//         // move to the text part after '{length}.'
//         const char *ptr = buffer + offset;
//         offset += len;
//
//         char delim = buffer[offset++];
//         if (delim != ',' && delim != ';')
//             return false;
//
//         if (first) {
//             instr.opcode = {len, ptr};
//
//             if (!IsOpcodeAllowed(instr.opcode)) {
//                 state = ParserState::DENIED_OPCODE;
//                 return false;
//             }
//
//             first = false;
//         } else {
//             if (instr.argc >= MAX_INSTRUCTION_ARGS)
//                 return false;
//
//             instr.argv[instr.argc++] = {len, ptr};
//         }
//
//         if (delim == ';') {
//             OnInstruction(instr);
//             return true;
//         }
//     }
// }
//
// bool GuacParser::Parse(const char *data, size_t len) {
//     if (used + len > capacity) {
//         state = ParserState::INVALID;
//         return false;
//     }
//
//     // Copy the data into the parser buffer
//     // TODO: check if maybe a safe alternative to function can be used
//     memcpy(buffer + used, data, len);
//     used += len;
//
//     size_t offset = 0;
//
//     //TODO: break condition
//     while (offset < used) {
//         size_t before = offset;
//
//         if (!ParseOne(offset)) {
//             // incomplete instruction
//             if (before == offset)
//                 break;
//
//             state = ParserState::INVALID;
//             return false;
//         }
//     }
//
//     // move remaining unprocessed bytes to buffer's start location
//     if (offset > 0) {
//         memmove(buffer, buffer + offset, used - offset);
//         used -= offset;
//     }
//
//     state = ParserState::READY;
//     return true;
// }
//
// bool GuacParser::IsOpcodeAllowed(const GuacElement& opcode)
//  {
//      return true;
//      // switch (opcode.len) {
//      //
//      //     case 3:
//      //         return !memcmp(opcode.ptr, "key", 3);
//      //
//      //     case 4:
//      //         return
//      //             !memcmp(opcode.ptr, "sync", 4) ||
//      //             !memcmp(opcode.ptr, "size", 4) ||
//      //             !memcmp(opcode.ptr, "blob", 4);
//      //
//      //     case 5:
//      //         return
//      //             !memcmp(opcode.ptr, "mouse", 5) ||
//      //             !memcmp(opcode.ptr, "audio", 5) ||
//      //             !memcmp(opcode.ptr, "video", 5);
//      //
//      //     default:
//      //         return false;
//      // }
//  } 
