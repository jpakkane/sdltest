project('SDL test', 'cpp', version: '1.0.0',
  default_options : ['default_library=static',
                     'cpp_std=c++17',
                     'buildtype=debugoptimized'])

sdl2_dep = dependency('sdl2')
if host_machine.system() == 'windows'
   sdl2_dep = [sdl2_dep, dependency('sdl2main')]
endif
sdl2_image_dep = dependency('SDL2_image')
sdl2_mixer_dep = dependency('SDL2_mixer')

executable('sdltestapp', 'main.cpp',
  dependencies : [sdl2_image_dep, sdl2_mixer_dep, sdl2_dep],
  win_subsystem: 'windows'
  )
