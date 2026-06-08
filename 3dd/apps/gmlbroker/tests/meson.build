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
