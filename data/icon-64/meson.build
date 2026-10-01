custom_target('io.github.bortoq.zatura_64.png',
  input: '../io.github.bortoq.zatura.svg',
  output: 'io.github.bortoq.zatura.png',
  command: [
    rsvg_convert,
    '-w', '64', '-h', '64',
    '-o', '@OUTPUT@',
    '@INPUT@'
  ],
  install: true,
  install_dir: join_paths(datadir, 'icons', 'hicolor', '64x64', 'apps')
)
