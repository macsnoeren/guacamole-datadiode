test_sources = files(
  'test_parser.cpp',
  '../src/guacparser.cpp'
)

incdirs = include_directories('../include')

test_exe = executable(
  'test_parser',
  sources: test_sources,
  include_directories: incdirs
)
test('parser', test_exe)
