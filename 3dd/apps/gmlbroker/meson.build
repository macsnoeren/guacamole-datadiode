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

executable(
  'gmlbroker',
  sources: sources,
  include_directories: incdir,
  )

subdir('tests')
