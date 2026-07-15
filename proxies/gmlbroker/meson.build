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

project(
  'gmlbroker',
  'cpp',
  version: '0.1',
  default_options: ['cpp_std=c++17']
  )

sources = [
  'src/main.cpp',
  'src/handshake_forger.cpp',
  'src/return_filter.cpp',
  'src/forward_keepalive_filter.cpp',
  'src/clipboard_ack_faker.cpp',
  'src/approval_registry.cpp',
  'src/channel_mailbox.cpp',
  'src/nethandlers/guacamole_accept_handler.cpp',
  'src/nethandlers/guacamole_read_handler.cpp',
  'src/nethandlers/guacamole_send_handler.cpp',
  'src/nethandlers/udp_recv_handler.cpp',
  'src/nethandlers/udp_send_handler.cpp',
  '../shared/src/network/udpsender.cpp',
  '../shared/src/network/udpreceiver.cpp',
  '../shared/src/network/guacamole_server.cpp',
  '../shared/src/network/multiplexer.cpp',
  '../shared/src/parser/opcode_parser.cpp',
  ]

incdir = include_directories('../shared/include/network')

# Prefer pkg-config (present in the Docker images via libssl-dev); fall back to
# locating libssl/libcrypto directly on hosts without an openssl .pc file.
openssl_dep = dependency('openssl', required: false)
if not openssl_dep.found()
  cc = meson.get_compiler('cpp')
  openssl_dep = [
    cc.find_library('ssl'),
    cc.find_library('crypto'),
  ]
endif

executable(
  'gmlbroker',
  sources: sources,
  include_directories: incdir,
  dependencies: openssl_dep,
  )

subdir('tests')
