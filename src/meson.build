mpsse_lib = shared_library('ftdi_mpsse',
  [ 'error.c', 'i2c.c', 'mpsse.c', 'spi.c' ],
  dependencies: ftdi,
  include_directories: [ '../include' ],
  install: true,
  version: meson.project_version())

mpsse = declare_dependency(link_with : mpsse_lib,
  include_directories : [ '../include' ],
  dependencies: ftdi)

pkg = import('pkgconfig')
pkg.generate(mpsse_lib,
  description : 'FTDI MPSSE library.')
