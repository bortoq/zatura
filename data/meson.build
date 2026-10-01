gnome = import('gnome')
zatura_resources = gnome.compile_resources(
  'resources',
  'zatura.gresource.xml',
  c_name: 'zatura_resources',
  dependencies: files('girara.css_t', 'zatura.css_t', 'io.github.bortoq.zatura.xml')
)

install_data('io.github.bortoq.zatura.xml', install_dir: dbusinterfacesdir)
install_data('io.github.bortoq.zatura.svg', install_dir: join_paths(datadir, 'icons', 'hicolor', 'scalable', 'apps'))

rsvg_convert = find_program('rsvg-convert', required: get_option('convert-icon'), native: true)
if rsvg_convert.found()
  foreach width : [16, 32, 64, 128, 256]
    subdir('icon-@0@'.format(width))
  endforeach
endif

i18n = import('i18n')
podir = join_paths(meson.project_source_root(), 'po')

desktop = i18n.merge_file(
  input: 'io.github.bortoq.zatura.desktop.in',
  output: 'io.github.bortoq.zatura.desktop',
  install: true,
  install_dir: desktopdir,
  po_dir: podir,
  type: 'desktop'
)

appdata = i18n.merge_file(
  input: 'io.github.bortoq.zatura.metainfo.xml.in',
  output: 'io.github.bortoq.zatura.metainfo.xml',
  install: true,
  install_dir: metainfodir,
  po_dir: podir,
)

desktop_file_validate = find_program('desktop-file-validate', required: false, native: true)
if desktop_file_validate.found()
  test('validate-desktop',
    desktop_file_validate,
    args: [desktop.full_path()]
  )
endif

appstreamcli = find_program('appstreamcli', required: false, native: true)
if appstreamcli.found()
  test(
    'validate-appdata',
    appstreamcli,
    args: ['validate', '--no-net', appdata.full_path()],
  )
endif

if get_option('shell-completions').allowed()
  conf_data = configuration_data()
  conf_data.set('PLUGINDIR', join_paths(prefix, plugindir))
  bash_completion = configure_file(
    input: 'bash-completion.in',
    output: 'zatura',
    configuration: conf_data
  )
  zsh_completion = configure_file(
    input: 'zsh-completion.in',
    output: '_zatura',
    configuration: conf_data
  )
  fish_completion = configure_file(
    input: 'fish-completion.in',
    output: 'zatura.fish',
    configuration: conf_data
  )

  bash_comp = dependency('bash-completion', required: false)
  if bash_comp.found()
    bash_compdir = bash_comp.get_variable(pkgconfig: 'completionsdir')
  else
    bash_compdir = join_paths(datadir, 'bash-completion', 'completions')
  endif

  fish_comp = dependency('fish', required: false)
  if fish_comp.found()
    fish_compdir = fish_comp.get_variable(pkgconfig: 'completionsdir')
  else
    fish_compdir = join_paths(datadir, 'fish', 'vendor_completions.d')
  endif

  install_data(bash_completion, install_dir: bash_compdir)
  install_data(zsh_completion, install_dir: join_paths(datadir, 'zsh', 'site-functions'))
  install_data(fish_completion, install_dir: fish_compdir)
endif
