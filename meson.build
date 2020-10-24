project('SDL test', 'cpp', version: '1.0.0',
  default_options : ['default_library=static',
                     'cpp_std=c++17',
                     'buildtype=debugoptimized'])

sdl2_dep = dependency('sdl2')

executable('sdltestapp', 'main.cpp',
  dependencies : [sdl2_dep],
  win_subsystem: 'windows')
