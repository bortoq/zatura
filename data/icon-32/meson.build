custom_target('io.github.bortoq.zatura_32.png',
  input: '../io.github.bortoq.zatura.svg',
  output: 'io.github.bortoq.zatura.png',
  command: [
    rsvg_convert,
    '-w', '32', '-h', '32',
    '-o', '@OUTPUT@',
    '@INPUT@'
  ],
  install: true,
  install_dir: join_paths(datadir, 'icons', 'hicolor', '32x32', 'apps')
)
