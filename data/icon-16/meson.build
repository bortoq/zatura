custom_target('io.github.bortoq.zatura_16.png',
  input: '../io.github.bortoq.zatura.svg',
  output: 'io.github.bortoq.zatura.png',
  command: [
    rsvg_convert,
    '-w', '16', '-h', '16',
    '-o', '@OUTPUT@',
    '@INPUT@'
  ],
  install: true,
  install_dir: join_paths(datadir, 'icons', 'hicolor', '16x16', 'apps')
)
