project(
  'gcdbroker',
  'cpp',
  version: '0.1',
  default_options: ['cpp_std=c++17']
  )

sources = [
  'src/main.cpp',
  'src/sync_faker.cpp',
  'src/nethandlers/guacd_send_handler.cpp',
  'src/nethandlers/guacd_read_handler.cpp',
  'src/nethandlers/udp_recv_handler.cpp',
  'src/nethandlers/udp_send_handler.cpp',
  '../shared/src/network/udpsender.cpp',
  '../shared/src/network/udpreceiver.cpp',
  '../shared/src/network/guacd_client.cpp',
  '../shared/src/network/multiplexer.cpp',
  '../shared/src/parser/opcode_parser.cpp',
  ]

incdir = include_directories('../shared/include/network')

executable(
  'gcdbroker',
  sources: sources,
  include_directories: incdir,
  )

subdir('tests')
