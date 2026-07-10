test_sources = files(
  'test_sync_faker.cpp',
  '../src/sync_faker.cpp',
  '../../shared/src/parser/opcode_parser.cpp',
)

incdirs = include_directories('../include')

test_exe = executable(
  'test_sync_faker',
  sources: test_sources,
  include_directories: incdirs,
)
test('sync_faker', test_exe)
