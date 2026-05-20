project(
  'udp_relay',
  'cpp',
  version: '0.1',
  default_options: ['cpp_std=c++17']
  )

sources = [
  'main.cpp',
  '../network/udpsender.cpp',
  '../network/udpreceiver.cpp',
  ]

incdir = include_directories('../../include/network')

executable(
  'udp_relay',
  sources: sources,
  include_directories: incdir,
  )
