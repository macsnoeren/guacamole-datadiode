# Guacamole Data Diode - Secure remote access using the Guacamole remote access using data-diodes.
# Copyright (C) 2020-2026  Maurice Snoeren, Simon de Cock
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: GPL-3.0-or-later

test_sources = files(
  'test_handshake_forger.cpp',
  '../src/handshake_forger.cpp',
  '../../shared/src/parser/opcode_parser.cpp'
)

test_exe = executable(
  'test_handshake_forger',
  sources: test_sources
)
test('handshake_forger', test_exe)

keepalive_filter_sources = files(
  'test_forward_keepalive_filter.cpp',
  '../src/forward_keepalive_filter.cpp',
  '../../shared/src/parser/opcode_parser.cpp'
)

keepalive_filter_exe = executable(
  'test_forward_keepalive_filter',
  sources: keepalive_filter_sources
)
test('forward_keepalive_filter', keepalive_filter_exe)

clipboard_ack_sources = files(
  'test_clipboard_ack_faker.cpp',
  '../src/clipboard_ack_faker.cpp',
  '../../shared/src/parser/opcode_parser.cpp'
)

clipboard_ack_exe = executable(
  'test_clipboard_ack_faker',
  sources: clipboard_ack_sources
)
test('clipboard_ack_faker', clipboard_ack_exe)
