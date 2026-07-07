#include "../include/forward_keepalive_filter.h"
#include <cassert>
#include <iostream>
#include <string>

// Run one chunk through the filter and return what would cross the bridge.
static std::string filtered(ForwardKeepaliveFilter &f, const std::string &s) {
    std::string buf = s;
    size_t len = buf.size();
    f.Filter(buf.data(), len);
    return std::string(buf.data(), len);
}

/*
 * @brief A standalone client sync or nop is swallowed entirely.
 */
void test_keepalive_only() {
    { ForwardKeepaliveFilter f; assert(filtered(f, "4.sync,7.1234567;") == ""); }
    { ForwardKeepaliveFilter f; assert(filtered(f, "3.nop;") == ""); }
}

/*
 * @brief sync/nop mixed with input are removed; the input survives.
 */
void test_keepalive_among_input() {
    ForwardKeepaliveFilter f;
    assert(filtered(f,
        "3.key,3.109,1.1;4.sync,2.31;3.nop;5.mouse,3.988,3.369,1.0;") ==
        "3.key,3.109,1.1;5.mouse,3.988,3.369,1.0;");
}

/*
 * @brief Non-keepalive input passes through untouched (the guard validates it).
 */
void test_passthrough() {
    ForwardKeepaliveFilter f;
    const std::string in = "5.mouse,3.988,3.369,1.0;3.key,3.109,1.1;";
    assert(filtered(f, in) == in);
}

/*
 * @brief A large forward element is not gmlbroker's call: it is tolerated and
 * passed through (the guard judges it), never corrupting the stream.
 */
void test_large_element_passthrough() {
    ForwardKeepaliveFilter f;
    std::string big(20000, 'a');
    std::string in = "4.blob,1.0," + std::to_string(big.size()) + "." + big + ";";
    assert(filtered(f, in) == in);
    // And a following keepalive is still swallowed.
    assert(filtered(f, "4.sync,1.9;") == "");
}

/*
 * @brief Fail open: a chunk that cannot be framed is left for the guard, and the
 * filter resyncs so later chunks are handled again.
 */
void test_fail_open_and_resync() {
    ForwardKeepaliveFilter f;
    const std::string garbage = "not guacamole";
    assert(filtered(f, garbage) == garbage); // untouched
    // Resynced: a clean keepalive afterwards is swallowed again.
    assert(filtered(f, "3.nop;") == "");
}

int main() {
    test_keepalive_only();
    test_keepalive_among_input();
    test_passthrough();
    test_large_element_passthrough();
    test_fail_open_and_resync();
    std::cout << "all ForwardKeepaliveFilter tests passed" << std::endl;
    return 0;
}
