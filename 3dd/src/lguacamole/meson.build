project(
  'lguacamole',
  'cpp',
  version: '0.1',
  default_options: ['cpp_std=c++17']
  )

sources = [
  'main.cpp',
  '../network/udpsender.cpp',
  '../network/udpreceiver.cpp',
  '../network/tcpserver.cpp',
  ]

incdir = include_directories('../../include/network')

executable(
  'lguacamole',
  sources: sources,
  include_directories: incdir,
  )
