project(
  'gmlbroker',
  'cpp',
  version: '0.1',
  default_options: ['cpp_std=c++17']
  )

sources = [
  'main.cpp',
  'handshake_forger.cpp',
  '../../shared/src/network/udpsender.cpp',
  '../../shared/src/network/udpreceiver.cpp',
  '../../shared/src/network/tcpserver.cpp',
  '../../shared/src/network/multiplexer.cpp',
  '../../shared/src/parser/opcode_parser.cpp',
  ]

incdir = include_directories('../../shared/include/network')

executable(
  'gmlbroker',
  sources: sources,
  include_directories: incdir,
  )

subdir('tests')
