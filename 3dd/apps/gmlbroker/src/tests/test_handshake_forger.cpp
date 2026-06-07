#include "../../include/handshake_forger.h"
#include "../../../shared/include/parser/opcode_parser.h"
#include <cassert>
#include <iostream>
#include <string>

// TODO: put the test/ directory in the same directory as src/

/**
 * @brief Accepts any opcode, so we can check the forger's *output* is well-formed
 * Guacamole (framing only), independent of the guard's allowlist.
 */
struct AnyOpcodeParser : OpcodeParser {
    bool OnInstructionBegin(const GuacElement &) override { return true; }
};

static bool well_formed(const std::string &s) {
    AnyOpcodeParser p;
    p.Parse(s.data(), s.size());
    return p.GetState() == ParserState::READY;
}

static bool contains(const std::string &hay, const std::string &needle) {
    return hay.find(needle) != std::string::npos;
}

/**
 * @brief select must produce a well-formed args reply and capture the protocol
 */
void test_select_triggers_args() {
    HandshakeForger f;
    std::string out = f.Feed("6.select,3.ssh;", 15);

    assert(f.GetHandshakeState() == HandshakeState::EXCHANGING_PARAMETERS);
    assert(f.Protocol() == "ssh");
    assert(contains(out, "4.args,"));
    assert(well_formed(out));
}

/**
 * @brief connect must produce ready + a waiting screen and capture connect values
 */
void test_connect_triggers_ready_and_screen() {
    HandshakeForger f;
    f.Feed("6.select,3.ssh;", 15);

    std::string size = "4.size,4.1680,3.933,2.96;";
    f.Feed(size.data(), size.size());

    std::string rest =
        "5.audio,8.audio/L8,9.audio/L16;5.video;5.image,10.image/jpeg;"
        "8.timezone,13.Europe/Berlin;4.name,9.guacadmin;"
        "7.connect,13.VERSION_1_5_0,9.localhost,0.,2.22;";
    std::string out = f.Feed(rest.data(), rest.size());

    assert(f.GetHandshakeState() == HandshakeState::ESTABLISHED);
    assert(contains(out, "5.ready,"));
    assert(contains(out, "4.size,"));  // waiting screen sizes layer 0
    assert(contains(out, "4.rect,"));
    assert(contains(out, "5.cfill,"));
    assert(well_formed(out));

    // connect values captured positionally
    assert(f.ConnectValues().size() >= 4);
    assert(f.ConnectValues()[0] == "VERSION_1_5_0");
    assert(f.ConnectValues()[1] == "localhost");
    assert(f.ConnectValues()[3] == "22");

    // forged id looks like $<uuid>
    assert(f.FakeId().size() == 37);
    assert(f.FakeId()[0] == '$');
}

/**
 * @brief A select split across two Feeds must still be parsed correctly
 */
void test_partial_select_across_feeds() {
    HandshakeForger f;
    std::string o1 = f.Feed("6.sel", 5);
    assert(o1.empty());
    assert(f.GetHandshakeState() == HandshakeState::UNESTABLISHED);

    std::string o2 = f.Feed("ect,3.ssh;", 10);
    assert(f.GetHandshakeState() == HandshakeState::EXCHANGING_PARAMETERS);
    assert(f.Protocol() == "ssh");
    assert(contains(o2, "4.args,"));
}

int main() {
    test_select_triggers_args();
    test_connect_triggers_ready_and_screen();
    test_partial_select_across_feeds();

    std::cout << "handshake_forger: all tests passed" << std::endl;
    return 0;
}
