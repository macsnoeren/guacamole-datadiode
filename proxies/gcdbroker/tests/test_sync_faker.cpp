#include "sync_faker.h"
#include <cassert>
#include <iostream>
#include <string>

// Feed a whole std::string in one chunk.
static std::string feed(SyncFaker &f, const std::string &s) {
    return f.Feed(s.data(), s.size());
}

/*
 * @brief A sync is echoed with guacd's own timestamp, verbatim.
 */
void test_basic_echo() {
    SyncFaker f;
    assert(feed(f, "4.sync,13.1234567890123;") == "4.sync,13.1234567890123;");
}

/*
 * @brief A sync embedded after other instructions is found; only it is echoed.
 */
void test_embedded() {
    SyncFaker f;
    assert(feed(f, "5.ready,3.abc;4.sync,2.31;") == "4.sync,2.31;");
}

/*
 * @brief A drawing element larger than the parser's element buffer
 * (MAX_ELEMENT_SIZE, 8192) is tolerated and framed past, and a following sync is
 * still recognised. This is the case that rules out the guard's strict parser.
 */
void test_large_element_skipped() {
    SyncFaker f;
    std::string big(20000, 'x'); // well over MAX_ELEMENT_SIZE
    std::string in = "4.blob,1.0," + std::to_string(big.size()) + "." + big +
                     ";4.sync,3.999;";
    assert(feed(f, in) == "4.sync,3.999;");
    // And the parser still tracks subsequent instructions cleanly.
    assert(feed(f, "4.sync,1.7;") == "4.sync,1.7;");
}

/*
 * @brief A non-sync (multi-argument) instruction produces no echo.
 */
void test_non_sync() {
    SyncFaker f;
    assert(feed(f, "3.key,3.109,1.1;") == "");
}

/*
 * @brief State persists across Feed() calls: a sync split mid-opcode and
 * mid-timestamp is still recognised.
 */
void test_split_across_reads() {
    {
        SyncFaker f;
        std::string e = feed(f, "4.sy");
        e += feed(f, "nc,2.31;");
        assert(e == "4.sync,2.31;");
    }
    {
        SyncFaker f;
        std::string e = feed(f, "4.sync,5.12");
        e += feed(f, "345;");
        assert(e == "4.sync,5.12345;");
    }
}

/*
 * @brief Several syncs in one chunk are all echoed, in order.
 */
void test_multiple() {
    SyncFaker f;
    assert(feed(f, "4.sync,1.1;5.image,3.png;4.sync,1.2;") ==
           "4.sync,1.1;4.sync,1.2;");
}

/*
 * @brief Zero-length elements (e.g. an empty connect argument) keep framing.
 */
void test_zero_length_elements() {
    SyncFaker f;
    assert(feed(f, "7.connect,0.,4.host;4.sync,1.7;") == "4.sync,1.7;");
}

/*
 * @brief Fail-open resync: a byte the FSM rejects (here a non-ASCII byte inside
 * an element) must not permanently stop echoing — a following sync still echoes.
 */
void test_recovers_from_corruption() {
    SyncFaker f;
    // A name carrying a non-ASCII (0xC3) byte would latch STREAM_CORRUPTED; the
    // sync after it must still be echoed.
    std::string in = "4.name,3.a";
    in.push_back('\xC3');
    in += "z;4.sync,1.9;";
    assert(feed(f, in) == "4.sync,1.9;");

    // And the faker keeps working on the next chunk (state was reset, not stuck).
    assert(feed(f, "4.sync,2.42;") == "4.sync,2.42;");
}

int main() {
    test_basic_echo();
    test_embedded();
    test_large_element_skipped();
    test_non_sync();
    test_split_across_reads();
    test_multiple();
    test_zero_length_elements();
    test_recovers_from_corruption();
    std::cout << "all SyncFaker tests passed" << std::endl;
    return 0;
}
