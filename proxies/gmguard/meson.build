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
  'guard',
  sources: release_sources,
  include_directories: incdirs,
)

subdir('tests')
