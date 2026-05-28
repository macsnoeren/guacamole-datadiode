project(
  'hrx_proxy',
  'cpp',
  version: '0.1',
  default_options: ['cpp_std=c++17']
  )

sources = [
  'main.cpp',
  '../../shared/network/udpsender.cpp',
  '../../shared/network/udpreceiver.cpp',
  ]

incdir = include_directories('../../shared/include/network')

executable(
  'hrx_proxy',
  sources: sources,
  include_directories: incdir,
  )
