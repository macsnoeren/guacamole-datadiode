project(
  'hguacamole',
  'cpp',
  version: '0.1',
  default_options: ['cpp_std=c++17']
  )

sources = [
  'main.cpp',
  '../../shared/network/udpsender.cpp',
  '../../shared/network/udpreceiver.cpp',
  '../../shared/network/tcpclient.cpp',
  ]

incdir = include_directories('../../shared/include/network')

executable(
  'hguacamole',
  sources: sources,
  include_directories: incdir,
  )
