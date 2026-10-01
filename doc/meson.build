sphinx = find_program('sphinx-build', required: get_option('manpages'))
if sphinx.found()
  custom_target('man pages',
    command: [
      sphinx,
      '-b', 'man',
      '-D', 'version=' + version,
      '-D', 'release=' + version,
      join_paths(meson.current_source_dir(), 'man'),
      meson.current_build_dir()],
    output: ['zatura.1', 'zaturarc.5'],
    input: [
      'man/conf.py',
      'man/zaturarc.5.rst',
      'man/zatura.1.rst'
    ],
    build_by_default: true,
    install: true,
    install_dir: [
      join_paths(get_option('mandir'), 'man1'),
      join_paths(get_option('mandir'), 'man5')
    ]
  )
  if seccomp.found() or landlock
    install_symlink('zatura-sandbox.1',
      install_dir: join_paths(get_option('mandir'), 'man1'),
      pointing_to: 'zatura.1'
    )
  endif

  meson.add_dist_script('build-man.sh')
else
  fs = import('fs')
  if fs.exists('zatura.1')
    install_man(files('zatura.1', 'zaturarc.5'))

    if seccomp.found() or landlock
      install_symlink('zatura-sandbox.1',
        install_dir: join_paths(get_option('mandir'), 'man1'),
        pointing_to: 'zatura.1'
      )
    endif
  endif
endif
