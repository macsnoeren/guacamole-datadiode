project(
  'udp_receiver',
  'cpp',
  version: '0.1',
  default_options: ['cpp_std=c++17']
  )

sources = [
  'main.cpp',
  '../network/udpreceiver.cpp'
  ]

incdir = include_directories('../../include/network')

executable(
  'udp_receiver',
  sources: sources,
  include_directories: incdir,
  )
