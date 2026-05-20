project(
  'udp_sender',
  'cpp',
  version: '0.1',
  default_options: ['cpp_std=c++17']
  )

sources = [
  'main.cpp',
  '../network/udpsender.cpp'
  ]

incdir = include_directories('../../include/network')

executable(
  'udp_sender',
  sources: sources,
  include_directories: incdir,
  )
