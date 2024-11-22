project('ftdi_mpsse', 'c',
  version: '1.0.0',
  default_options: [
    'optimization=g',
    'warning_level=2',
    'b_lto=true',
    'b_pie=true',
    'c_std=gnu17',
  ])

add_project_arguments('-ggdb', language: 'c')

ftdi = dependency('libftdi1')

subdir('include')
subdir('src')

subdir('examples')
subdir('tools')
