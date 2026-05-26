project(
  'guard',
  'cpp',
  version: '0.1',
  default_options: ['cpp_std=c++17']
)

core_sources = files(
  'src/guacparser.cpp',
)

release_sources = core_sources + files('src/main.cpp', '../shared/network/udpsender.cpp', '../shared/network/udpreceiver.cpp')

incdirs = include_directories(
  'include',
  '../../shared/include/network',
)

executable(
  'guard',
  sources: release_sources,
  include_directories: incdirs,
)

test_sources = core_sources + files('tests/main.cpp')

test_exe = executable(
  'tests',
  sources: test_sources,
  include_directories: incdirs
)
test('tests_main', test_exe)
