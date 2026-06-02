#include "../include/handshake_forger.h"
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

// Build one Guacamole instruction from its elements: "len.elem,len.elem,...;"
std::string instr(const std::vector<std::string> &elems) {
    std::string s;
    for (size_t i = 0; i < elems.size(); ++i) {
        s += std::to_string(elems[i].size());
        s += '.';
        s += elems[i];
        s += (i + 1 < elems.size()) ? ',' : ';';
    }
    return s;
}

// A v4-style UUID prefixed with '$', matching guacd's connection-id format
// (e.g. "$79fce574-83ce-4418-a9e5-949b5c4e482b", 37 chars).
std::string make_fake_id() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<int> hex(0, 15);
    const char *digits = "0123456789abcdef";
    std::string id = "$";
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20)
            id += '-';
        id += digits[hex(rng)];
    }
    return id;
}

// Canned `args` reply for ssh, captured verbatim from guacd 1.6.0. The ordering
// matters: the client's `connect` values are positional against this list.
const char *SSH_ARGS_1_6_0 =
    "4.args,13.VERSION_1_5_0,8.hostname,8.host-key,4.port,8.username,8.password,"
    "9.font-name,9.font-size,11.enable-sftp,19.sftp-root-directory,"
    "21.sftp-disable-download,19.sftp-disable-upload,11.private-key,"
    "10.passphrase,12.color-scheme,7.command,15.typescript-path,"
    "15.typescript-name,22.create-typescript-path,14.recording-path,"
    "14.recording-name,24.recording-exclude-output,23.recording-exclude-mouse,"
    "22.recording-include-keys,21.create-recording-path,9.read-only,"
    "21.server-alive-interval,9.backspace,13.terminal-type,10.scrollback,"
    "6.locale,8.timezone,12.disable-copy,13.disable-paste,15.wol-send-packet,"
    "12.wol-mac-addr,18.wol-broadcast-addr,12.wol-udp-port,13.wol-wait-time;";

} // namespace

std::string HandshakeForger::Feed(const char *data, size_t len) {
    out.clear();
    Parse(data, len);
    if (GetState() == ParserState::INVALID)
        hs_state = HandshakeState::INVALID_HANDSHAKE;
    return out;
}

bool HandshakeForger::OnInstructionBegin(const GuacElement &instr) {
    current_opcode.assign(instr.ptr, instr.len);
    std::cout << "Received opcode " << current_opcode << std::endl;
    // The forger is not a security gate; accept every handshake opcode.
    return true;
}

bool HandshakeForger::OnArgument(const GuacElement &arg) {
    if (current_opcode == "select")
        protocol.assign(arg.ptr, arg.len);
    else if (current_opcode == "connect")
        connect_values.emplace_back(arg.ptr, arg.len);
    else if (current_opcode == "size")
        size_args.emplace_back(arg.ptr, arg.len);
    return true;
}

bool HandshakeForger::OnInstructionEnd() {
    if (current_opcode == "select") {
        out += CannedArgs();
        hs_state = HandshakeState::EXCHANGING_PARAMETERS;
    } else if (current_opcode == "connect") {
        fake_id = make_fake_id();
        out += instr({"ready", fake_id});
        out += WaitingScreen();
        hs_state = HandshakeState::ESTABLISHED;
    }
    return true;
}

std::string HandshakeForger::CannedArgs() const {
    // Only ssh is wired up for the PoC; extend per protocol as needed.
    if (protocol == "ssh")
        return SSH_ARGS_1_6_0;
    return SSH_ARGS_1_6_0; // PoC fallback
}

std::string HandshakeForger::WaitingScreen() const {
    // PROVISIONAL: paint the default layer a solid colour. The exact compositing
    // mode and argument order are to be confirmed against the 1.6.0 client when
    // this is wired into gmlbroker (slice 1b); here it is only unit-tested for
    // well-formedness.
    std::string w = size_args.size() > 0 ? size_args[0] : "1024";
    std::string h = size_args.size() > 1 ? size_args[1] : "768";

    std::string s;
    s += instr({"size", "0", w, h});                  // size default layer (0)
    s += instr({"rect", "0", "0", "0", w, h});        // full-layer rectangle
    s += instr({"cfill", "14", "0", "40", "40", "40", "255"}); // OVER, dark grey
    s += instr({"sync", "0"});                         // close the frame
    return s;
}
