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
  'gmguard',
  'cpp',
  version: '0.1',
  default_options: ['cpp_std=c++17']
)

core_sources = files(
  '../shared/src/parser/opcode_parser.cpp',
)

release_sources = core_sources + files(
  'src/main.cpp',
  'src/approver.cpp',
  'src/guard_opcode_parser.cpp',
  '../shared/src/network/udpsender.cpp',
  '../shared/src/network/udpreceiver.cpp',
  '../shared/src/network/multiplexer.cpp')

incdirs = include_directories(
  'include',
  '../shared/include/network',
  '../shared/include/parser',
)

executable(
  'gmguard',
  sources: release_sources,
  include_directories: incdirs,
)

subdir('tests')
