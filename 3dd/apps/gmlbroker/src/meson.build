project(
  'gmlbroker',
  'cpp',
  version: '0.1',
  default_options: ['cpp_std=c++17']
  )

sources = [
  'main.cpp',
  'handshake_forger.cpp',
  'return_filter.cpp',
  'approval_registry.cpp',
  'nethandlers/guacamole_accept_handler.cpp',
  'nethandlers/tcp_read_handler.cpp',
  'nethandlers/tcp_send_handler.cpp',
  'nethandlers/udp_recv_handler.cpp',
  'nethandlers/udp_send_handler.cpp',
  '../../shared/src/network/udpsender.cpp',
  '../../shared/src/network/udpreceiver.cpp',
  '../../shared/src/network/guacamole_server.cpp',
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
