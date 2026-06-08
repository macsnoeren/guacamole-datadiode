project(
  'gcdbroker',
  'cpp',
  version: '0.1',
  default_options: ['cpp_std=c++17']
  )

sources = [
  'main.cpp',
  'nethandlers/tcp_send_handler.cpp',
  'nethandlers/tcp_read_handler.cpp',
  'nethandlers/udp_recv_handler.cpp',
  'nethandlers/udp_send_handler.cpp',
  '../../shared/src/network/udpsender.cpp',
  '../../shared/src/network/udpreceiver.cpp',
  '../../shared/src/network/tcpclient.cpp',
  '../../shared/src/network/multiplexer.cpp',
  ]

incdir = include_directories('../../shared/include/network')

executable(
  'gcdbroker',
  sources: sources,
  include_directories: incdir,
  )
