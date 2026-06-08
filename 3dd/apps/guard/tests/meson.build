test_sources = files(
  'test_parser.cpp',
  '../../shared/src/parser/opcode_parser.cpp'
)

incdirs = include_directories('../include', '../../shared/include/parser')

test_exe = executable(
  'test_parser',
  sources: test_sources,
  include_directories: incdirs
)
test('parser', test_exe)
