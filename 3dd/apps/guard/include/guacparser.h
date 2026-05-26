#pragma once

#include <cstddef>
#include <cstdint>

#define MAX_ELEMENT_SIZE 8192

enum class ParserState {
    READY,
    PARSING,
    INVALID,
    DENIED_OPCODE
};

enum class ParserPhase {
    READING_LENGTH,
    READING_DATA,
    EXPECT_DELIM
};

struct GuacElement {
    uint32_t len;
    const char *ptr;
};

// struct GuacInstruction {
//     GuacElement opcode;
//     uint32_t argc;
//     GuacElement argv[MAX_INSTRUCTION_ARGS];
// };

class GuacParser {
    public:
        GuacParser() = default;

        ParserState GetState() { return state; };
        ParserState Parse(const char *data, size_t len);
        void Reset();

        // bool ParseOne(size_t &offset);
        //
        // bool IsOpcodeAllowed(const GuacElement &opcode);

    protected:
        virtual bool OnInstructionBegin(const GuacElement &instr);
        virtual bool OnArgument(const GuacElement &arg) {return true;}
        virtual bool OnInstructionEnd() {return true;}

    private:
        ParserState state = ParserState::READY;
        ParserPhase phase = ParserPhase::READING_LENGTH;

        uint32_t current_length = 0;
        uint32_t current_read = 0;
        bool reading_opcode = true;
        char element_buffer[MAX_ELEMENT_SIZE];
};
