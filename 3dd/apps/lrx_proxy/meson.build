project(
  'lrx_proxy',
  'cpp',
  version: '0.1',
  default_options: ['cpp_std=c++17']
  )

sources = [
  'src/main.cpp',
  '../shared/src/network/udpsender.cpp',
  '../shared/src/network/udpreceiver.cpp',
  ]

incdir = include_directories('../shared/include/network')

executable(
  'lrx_proxy',
  sources: sources,
  include_directories: incdir,
  )
