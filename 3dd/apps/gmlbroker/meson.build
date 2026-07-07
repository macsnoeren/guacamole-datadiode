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
