project(
  'guard',
  'cpp',
  version: '0.1',
  default_options: ['cpp_std=c++17']
)

core_sources = files(
  'src/guacparser.cpp',
)

release_sources = core_sources + files(
  'src/main.cpp',
  '../shared/src/network/udpsender.cpp',
  '../shared/src/network/udpreceiver.cpp',
  '../shared/src/network/multiplexer.cpp')

incdirs = include_directories(
  'include',
  '../shared/include/network',
)

executable(
  'guard',
  sources: release_sources,
  include_directories: incdirs,
)

subdir('tests')
