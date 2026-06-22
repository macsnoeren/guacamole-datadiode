#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#define MAX_ELEMENT_SIZE 8192

/*
 * @brief Every state of the opcode-parsing FSM.
 *
 * The single state captures where in an
 * element the parser sits. Each byte advances exactly
 * one state. READING_LENGTH for reading element length,
 * READING_DATA is for reading data after the .-character
 * (only if the length is valid). STREAM_CORRUPTED occurs
 * when the parser received non-Guacamole data, or the Guacamole
 * data had an invalid format (not <length>.<value><, or ;>)
 * and DENIED_DATA occurs when an opcode or argument is disallowed.
 * Disallowing an opcode is not functionality of OpcodeParser, but
 * left for the inheriting GuardOpcodeParser class (which does the
 * filtering).
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
     * @brief Resets the framing state to a clean instruction boundary.
     *
     * Clears the FSM, the in-flight element, and any recorded denied ranges so
     * the next Parse() starts fresh — e.g. to recover (fail open) after a
     * STREAM_CORRUPTED.
     */
    void Reset();

  protected:
    /*
     * @brief Called whenever an instruction (opcode) is received by the parser
     *
     * The base parser is a neutral Guacamole framer and allows every opcode; the
     * opcode allowlist is a guard policy and lives in `GuardOpcodeParser`. A
     * subclass overrides this to reject an instruction (the base then records its
     * byte range for excision).
     *
     * @param instr: the struct containing the opcode
     * @return Whether or not an instruction is allowed
     */
    virtual bool OnInstructionBegin(const GuacElement &instr);

    /*
     * @brief Policy for an element whose declared length exceeds the element
     * buffer (MAX_ELEMENT_SIZE).
     *
     * The base treats an oversized element as untrustworthy — the guard's
     * behaviour: STREAM_CORRUPTED. A subclass that legitimately sees large
     * elements — e.g. gcdbroker scanning guacd's drawing output — overrides this
     * to return true; the parser then buffers only the first MAX_ELEMENT_SIZE
     * bytes (so the hooks see a clamped element) and frames past the rest.
     *
     * @return true to tolerate (skip the surplus of) oversized elements, false
     *         to corrupt the stream.
     */
    virtual bool ToleratesOversizedElements() { return false; }

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

    // Declared length of the value being parsed. 64-bit because a tolerated
    // oversized element (see ToleratesOversizedElements) can be far larger than
    // the buffer, and the true length is needed to know where the element ends.
    long long current_length = -1;

    // Number of value bytes read so far (counts the whole element, even when only
    // the first MAX_ELEMENT_SIZE are buffered).
    long long current_read = 0;
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
