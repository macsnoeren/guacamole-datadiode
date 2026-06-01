#pragma once

#include <cstddef>
#include <cstdint>

#define MAX_ELEMENT_SIZE 8192
#define CLIPBOARD_MAX_BYTES 14

enum class ParserState { READY, PARSING, INVALID, DENIED_OPCODE };

enum class ParserPhase { READING_LENGTH, READING_DATA, EXPECT_DELIM };

struct GuacElement {
    uint32_t len;
    const char *ptr;
};

class GuacParser {
  public:
    GuacParser() = default;

    ParserState GetState() { return state; };

    /*
     * @brief Parses and checks if Guacamole traffic is valid and allowed
     * @param data: *buffer to the payload to parse
     * @param len: length of the payload
     * @return the state of the parser after parsing
     */
    ParserState Parse(const char *data, size_t len);

    /*
     * TODO: implement
     */
    void Reset();

  protected:
    /*
     * @brief Called whenever an instruction (opcode) is received by the parser
     * @param instr: the struct containing the opcode
     * @return Whether or not an instruction is allowed
     */
    virtual bool OnInstructionBegin(const GuacElement &instr);

    /*
     * @brief Called whenever an argument is received by the parser
     * @param arg: the struct containing the argument
     * @return Whether or not the argument is valid
     */
    virtual bool OnArgument(const GuacElement &arg) { return true; }

    /*
     * @brief Called whenever the instruction (opcode) has finished parsing
     */
    virtual bool OnInstructionEnd() { return true; }

    // Variable maximum length of the next argument, for limiting clipboard payloads
    uint32_t next_args_max_length = 0;

  private:
    ParserState state = ParserState::READY;
    ParserPhase phase = ParserPhase::READING_LENGTH;

    // Current length of the value being parsed
    int current_length = -1;

    // Amount of bytes that is being read
    uint32_t current_read = 0;
    bool reading_opcode = true;

    // Storage for the current opcode or argument being parsed
    char element_buffer[MAX_ELEMENT_SIZE];
};
