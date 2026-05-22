project(
  'ltx_proxy',
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
  'ltx_proxy',
  sources: sources,
  include_directories: incdir,
  )
