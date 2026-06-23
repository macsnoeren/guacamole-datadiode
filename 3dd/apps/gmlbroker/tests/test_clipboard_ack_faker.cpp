#include "../include/clipboard_ack_faker.h"
#include <cassert>
#include <iostream>
#include <string>

static std::string feed(ClipboardAckFaker &f, const std::string &s) {
    return f.Feed(s.data(), s.size());
}

// Build a `blob,<idx>,<payload>` whose payload is `payloadlen` bytes.
static std::string blob(int idx, int payloadlen) {
    std::string p(payloadlen, 'a');
    std::string si = std::to_string(idx), pl = std::to_string(payloadlen);
    return "4.blob," + std::to_string(si.size()) + "." + si + "," + pl + "." +
           p + ";";
}

static std::string clip_open(int idx) {
    std::string si = std::to_string(idx);
    return "9.clipboard," + std::to_string(si.size()) + "." + si +
           ",10.text/plain;";
}

// A blob within the cap is forwarded and acked by guacd — no fake ack.
void test_small_blob_no_ack() {
    ClipboardAckFaker f;
    assert(feed(f, clip_open(0) + blob(0, 40) + "3.end,1.0;") == "");
    // Exactly at the cap (66 base64) is still allowed.
    ClipboardAckFaker g;
    assert(feed(g, clip_open(0) + blob(0, 66)) == "");
}

// An oversized clipboard blob (the guard will drop it) gets a faked success ack.
void test_oversized_blob_acked() {
    ClipboardAckFaker f;
    assert(feed(f, clip_open(0) + blob(0, 67)) == "3.ack,1.0,2.OK,1.0;");
    ClipboardAckFaker g; // large paste, different stream index
    assert(feed(g, clip_open(3) + blob(3, 5000)) == "3.ack,1.3,2.OK,1.0;");
}

// An oversized blob that is NOT on the open clipboard stream (e.g. a file
// upload) is left alone — only clipboard pastes are faked.
void test_off_stream_blob_ignored() {
    ClipboardAckFaker f;
    assert(feed(f, clip_open(0) + blob(1, 5000)) == "");
}

// Other input around the paste doesn't confuse the scanner.
void test_interleaved_input() {
    ClipboardAckFaker f;
    assert(feed(f, "3.key,3.109,1.1;" + clip_open(0) + "5.mouse,3.1,3.2,1.0;" +
                       blob(0, 200)) == "3.ack,1.0,2.OK,1.0;");
}

// State persists across reads: a blob split from its clipboard open.
void test_split_across_reads() {
    ClipboardAckFaker f;
    std::string e = feed(f, clip_open(2));
    e += feed(f, blob(2, 300));
    assert(e == "3.ack,1.2,2.OK,1.0;");
}

// Once the stream is closed by `end`, a later blob on that index is not acked.
void test_closed_stream() {
    ClipboardAckFaker f;
    assert(feed(f, clip_open(0) + "3.end,1.0;" + blob(0, 300)) == "");
}

int main() {
    test_small_blob_no_ack();
    test_oversized_blob_acked();
    test_off_stream_blob_ignored();
    test_interleaved_input();
    test_split_across_reads();
    test_closed_stream();
    std::cout << "all ClipboardAckFaker tests passed" << std::endl;
    return 0;
}
