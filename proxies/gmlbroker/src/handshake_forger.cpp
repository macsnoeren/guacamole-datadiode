/*
 * Guacamole Data Diode - Secure remote access using the Guacamole remote access using data-diodes.
 * Copyright (C) 2020-2026  Maurice Snoeren, Simon de Cock
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "../include/handshake_forger.h"
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

// Canned `args` reply for ssh, captured verbatim from guacd 1.6.0 (43 elements:
// the VERSION token plus 42 parameter names). The count and ordering must match
// guacd exactly: the browser builds `connect` with one value per arg in this
// list, and the real guacd validates that count when the handshake is replayed.
// guacd 1.6.0 adds `timeout`, `public-key`, `typescript-write-existing`, and
// `recording-write-existing` over the 1.5.0 list.
const char *SSH_ARGS_1_6_0 =
    "4.args,13.VERSION_1_5_0,8.hostname,8.host-key,4.port,7.timeout,8.username,"
    "8.password,9.font-name,9.font-size,11.enable-sftp,19.sftp-root-directory,"
    "21.sftp-disable-download,19.sftp-disable-upload,11.private-key,"
    "10.passphrase,10.public-key,12.color-scheme,7.command,15.typescript-path,"
    "15.typescript-name,22.create-typescript-path,25.typescript-write-existing,"
    "14.recording-path,14.recording-name,24.recording-exclude-output,"
    "23.recording-exclude-mouse,22.recording-include-keys,"
    "21.create-recording-path,24.recording-write-existing,9.read-only,"
    "21.server-alive-interval,9.backspace,13.terminal-type,10.scrollback,"
    "6.locale,8.timezone,12.disable-copy,13.disable-paste,15.wol-send-packet,"
    "12.wol-mac-addr,18.wol-broadcast-addr,12.wol-udp-port,13.wol-wait-time;";

// Canned `args` reply for rdp, captured verbatim from guacd 1.6.0 (89 elements:
// the VERSION token plus 88 parameter names). Same rule as ssh: the count and
// ordering must match guacd exactly, since the browser's `connect` is positional
// against this list and the real guacd validates the count on replay.
const char *RDP_ARGS_1_6_0 =
    "4.args,13.VERSION_1_5_0,8.hostname,4.port,7.timeout,6.domain,"
    "8.username,8.password,5.width,6.height,3.dpi,15.initial-program,"
    "11.color-depth,13.disable-audio,15.enable-printing,12.printer-name,"
    "12.enable-drive,10.drive-name,10.drive-path,17.create-drive-path,"
    "16.disable-download,14.disable-upload,7.console,13.console-audio,"
    "13.server-layout,8.security,11.ignore-cert,9.cert-tofu,"
    "17.cert-fingerprints,12.disable-auth,10.remote-app,"
    "14.remote-app-dir,15.remote-app-args,15.static-channels,"
    "11.client-name,16.enable-wallpaper,14.enable-theming,"
    "21.enable-font-smoothing,23.enable-full-window-drag,"
    "26.enable-desktop-composition,22.enable-menu-animations,"
    "22.disable-bitmap-caching,25.disable-offscreen-caching,"
    "21.disable-glyph-caching,11.disable-gfx,16.preconnection-id,"
    "18.preconnection-blob,8.timezone,11.enable-sftp,13.sftp-hostname,"
    "13.sftp-host-key,9.sftp-port,12.sftp-timeout,13.sftp-username,"
    "13.sftp-password,16.sftp-private-key,15.sftp-passphrase,"
    "15.sftp-public-key,14.sftp-directory,19.sftp-root-directory,"
    "26.sftp-server-alive-interval,21.sftp-disable-download,"
    "19.sftp-disable-upload,14.recording-path,14.recording-name,"
    "24.recording-exclude-output,23.recording-exclude-mouse,"
    "23.recording-exclude-touch,22.recording-include-keys,"
    "21.create-recording-path,24.recording-write-existing,"
    "13.resize-method,18.enable-audio-input,12.enable-touch,9.read-only,"
    "16.gateway-hostname,12.gateway-port,14.gateway-domain,"
    "16.gateway-username,16.gateway-password,17.load-balance-info,"
    "12.disable-copy,13.disable-paste,15.wol-send-packet,12.wol-mac-addr,"
    "18.wol-broadcast-addr,12.wol-udp-port,13.wol-wait-time,"
    "14.force-lossless,19.normalize-clipboard;";

} // namespace

std::string HandshakeForger::Feed(const char *data, size_t len) {
    out.clear();
    // Keep a verbatim copy of the handshake so it can be replayed to the real
    // guacd once approved. (Feed is only called before ESTABLISHED.)
    handshake_raw.append(data, len);
    Parse(data, len);
    if (GetState() == ParserState::STREAM_CORRUPTED)
        hs_state = HandshakeState::INVALID_HANDSHAKE;
    return out;
}

bool HandshakeForger::OnInstructionBegin(const GuacElement &instr) {
    current_opcode.assign(instr.ptr, instr.len);
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
    // After select was sent, send fake connection parameters
    if (current_opcode == "select") {
        out += CannedArgs();
        hs_state = HandshakeState::EXCHANGING_PARAMETERS;
    } else if (current_opcode == "connect") {
        // When connect is received send a ready instruction with a fake ID
        fake_id = make_fake_id();
        out += instr({"ready", fake_id});
        out += WaitingScreen();
        hs_state = HandshakeState::ESTABLISHED;
    }
    return true;
}

std::string HandshakeForger::CannedArgs() const {
    // Per-protocol canned args, pinned to guacd 1.6.0. Extend as more protocols
    // are wired up; the forged list must match guacd's real args exactly.
    if (protocol == "rdp")
        return RDP_ARGS_1_6_0;
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

std::string HandshakeForger::DeniedScreen() {
    // Generous default dimensions: exact size is unimportant for an error
    // overlay. Paint the default layer solid red, then ask the client to leave.
    std::string s;
    s += instr({"size", "0", "1024", "768"});
    s += instr({"rect", "0", "0", "0", "1024", "768"});
    s += instr({"cfill", "14", "0", "180", "30", "30", "255"}); // OVER, red
    s += instr({"sync", "0"});
    s += instr({"disconnect"});
    return s;
}
