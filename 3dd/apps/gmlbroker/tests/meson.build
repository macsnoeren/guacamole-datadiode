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
