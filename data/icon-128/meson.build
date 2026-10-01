custom_target('io.github.bortoq.zatura_128.png',
  input: '../io.github.bortoq.zatura.svg',
  output: 'io.github.bortoq.zatura.png',
  command: [
    rsvg_convert,
    '-w', '128', '-h', '128',
    '-o', '@OUTPUT@',
    '@INPUT@'
  ],
  install: true,
  install_dir: join_paths(datadir, 'icons', 'hicolor', '128x128', 'apps')
)
