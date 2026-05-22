project(
  'hguacamole',
  'cpp',
  version: '0.1',
  default_options: ['cpp_std=c++17']
  )

sources = [
  'main.cpp',
  '../network/udpsender.cpp',
  '../network/udpreceiver.cpp',
  '../network/tcpclient.cpp',
  ]

incdir = include_directories('../../include/network')

executable(
  'hguacamole',
  sources: sources,
  include_directories: incdir,
  )
