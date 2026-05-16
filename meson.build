project('yuris-player', 'c',
  version: '0.0.1',
  default_options: ['c_std=gnu99', 'warning_level=1']
)

sdl2_dep = dependency('sdl2')

cc = meson.get_compiler('c')
iconv_dep = dependency('iconv', required: false)
if not iconv_dep.found()
  if cc.has_function('iconv', prefix: '#include <iconv.h>')
    iconv_dep = declare_dependency()
  else
    iconv_dep = cc.find_library('iconv', required: true)
  endif
endif
m_dep = cc.find_library('m', required: false)


libyuris_dir = meson.current_source_dir() / 'libyuris/target/release'
libyuris_path = libyuris_dir / 'libyuris_decomp.so'

cargo_build = custom_target(
  'yuris_decomp',
  output: 'yuris_decomp.stamp',
  command: [
    'cargo', 'build', '--release',
    '--manifest-path', meson.current_source_dir() / 'libyuris/Cargo.toml'
  ],
  build_by_default: true,
)

libyuris_dep = declare_dependency(
  sources: cargo_build,
  include_directories: include_directories('include'),
  link_args: [
    '-L' + libyuris_dir,
    '-lyuris_decomp',
    '-Wl,-rpath,' + libyuris_dir,
  ],
)

inc = include_directories('include')

src_files = files(
  'src/yurisplayer.c',
  'src/archive.c',
  'src/script_reader.c',
  'src/utils.c',
  'src/encoding.c',
  'src/expr.c',
  'src/debug.c'
)

if get_option('buildtype') in ['debug', 'debugoptimized']
    add_project_arguments('-DYURIS_DEBUG', language: 'c')
endif

executable('yuris-player',
  src_files,
  include_directories: inc,
  dependencies: [libyuris_dep, sdl2_dep, iconv_dep, m_dep],
)
