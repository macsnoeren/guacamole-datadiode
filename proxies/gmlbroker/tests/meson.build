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
