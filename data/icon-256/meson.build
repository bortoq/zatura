custom_target('io.github.bortoq.zatura_256.png',
  input: '../io.github.bortoq.zatura.svg',
  output: 'io.github.bortoq.zatura.png',
  command: [
    rsvg_convert,
    '-w', '256', '-h', '256',
    '-o', '@OUTPUT@',
    '@INPUT@'
  ],
  install: true,
  install_dir: join_paths(datadir, 'icons', 'hicolor', '256x256', 'apps')
)
