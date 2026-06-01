project(
  'gcdbroker',
  'cpp',
  version: '0.1',
  default_options: ['cpp_std=c++17']
  )

sources = [
  'main.cpp',
  '../../shared/src/network/udpsender.cpp',
  '../../shared/src/network/udpreceiver.cpp',
  '../../shared/src/network/tcpclient.cpp',
  ]

incdir = include_directories('../../shared/include/network')

executable(
  'gcdbroker',
  sources: sources,
  include_directories: incdir,
  )
