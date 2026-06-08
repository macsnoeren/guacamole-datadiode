#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#define MAX_ELEMENT_SIZE 8192
#define CLIPBOARD_MAX_BYTES 14

/*
 * @brief Every state of the opcode-parsing FSM.
 *
 * There is no separate "phase": the single state captures both where in an
 * element the parser sits and any terminal verdict. Each byte advances exactly
 * one state. The diagram also draws two transient states — READING_DOT (the '.'
 * closing a length) and HANDLING_ELEMENT (a complete element to validate) — but
 * they consume no input of their own, so the code folds them into the state
 * that consumes their trigger byte (READING_LENGTH's '.' branch and
 * EXPECT_DELIM respectively). STREAM_CORRUPTED and DENIED_DATA are terminal for
 * the current Parse() call.
 */
enum class ParserState {
    READING_LENGTH,   // reading the digits of an element's length prefix
    READING_DATA,     // reading the value bytes
    EXPECT_DELIM,     // expecting ',' or ';' to close the element
    STREAM_CORRUPTED, // unparseable byte: the stream can no longer be trusted
    DENIED_DATA       // a disallowed opcode or argument value
};

struct GuacElement {
    uint32_t len;
    const char *ptr;
};

class OpcodeParser {
  public:
    OpcodeParser() = default;

    ParserState GetState() { return state; };

    /*
     * @brief Analyses Guacamole traffic and checks it is valid and allowed.
     *
     * Pure analysis: the buffer is never modified. When an instruction is
     * disallowed the parser records the byte range it occupies (so the caller
     * can drop it with Excise) and keeps analysing the rest. The return value is
     * DENIED_DATA when at least one instruction was denied and the rest parsed
     * cleanly; STREAM_CORRUPTED when the stream can no longer be trusted;
     * otherwise the resting FSM state.
     *
     * @param data: buffer to parse (read-only)
     * @param len: length of the payload
     * @return the state of the parser after parsing
     */
    ParserState Parse(const char *data, size_t len);

    /*
     * @brief Removes the instructions the last Parse() denied from a buffer.
     *
     * The caller passes the same buffer/length it gave the matching Parse()
     * call; the denied byte ranges are shifted out in place and `len` is reduced
     * accordingly. This is the only mutation path — Parse() itself never writes
     * to the buffer. A no-op when the last Parse() denied nothing.
     *
     * @param data: send buffer to compact in place
     * @param len: length of the buffer; reduced by the excised bytes
     */
    void Excise(char *data, size_t &len);

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

    /*
     * @brief Index, within the current Parse() buffer, of the byte being
     * processed. Valid inside the OnInstructionBegin/End and OnArgument hooks,
     * e.g. to find where the terminating delimiter of an instruction sits.
     */
    size_t CurrentIndex() const { return current_index; }

  private:
    ParserState state = ParserState::READING_LENGTH;

    // Index of the byte currently being processed in Parse()
    size_t current_index = 0;

    // Current length of the value being parsed
    int current_length = -1;

    // Amount of bytes that is being read
    uint32_t current_read = 0;
    bool reading_opcode = true;

    // Storage for the current opcode or argument being parsed
    char element_buffer[MAX_ELEMENT_SIZE];

    // Start index (within the current Parse() buffer) of the instruction
    // currently being parsed, so its full range can be recorded when denied.
    size_t opcode_start_idx = 0;

    // Byte ranges [start, length) of the instructions the last Parse() denied,
    // in ascending order. Populated by Parse(), consumed and cleared by Excise().
    std::vector<std::pair<size_t, size_t>> denied_ranges;
};
