project('zatura', 'c',
  version: '2026.10.06.1',
  meson_version: '>=1.6',
  default_options: ['c_std=c23', 'warning_level=3'],
)

version = meson.project_version()
version_array = version.split('.')

# Rules for plugin API and ABI (non-exhaustive):
# * zathura_plugin_functions_t: If functions are added or removed or their
#   signature changes, bump both ABI and API.
# * zathura_plugin_definition_t: If the struct changes in an ABI-incompatible
#   way, bump the ABI.
plugin_api_version = '8'
plugin_abi_version = '9'

conf_data = configuration_data()
conf_data.set('ZVMAJOR', version_array[0])
conf_data.set('ZVMINOR', version_array[1])
conf_data.set('ZVREV', version_array[2])
conf_data.set('ZVFULL', version)
conf_data.set('ZVAPI', plugin_api_version)
conf_data.set('ZVABI', plugin_abi_version)

cc = meson.get_compiler('c')

prefix = get_option('prefix')
localedir = get_option('localedir')
datadir = get_option('datadir')
metainfodir = join_paths(datadir, 'metainfo')
desktopdir = join_paths(datadir, 'applications')
dbusinterfacesdir = join_paths(datadir, 'dbus-1', 'interfaces')
plugindir = join_paths(get_option('libdir'), 'zatura')
sysconfdir = get_option('sysconfdir')
if not sysconfdir.startswith('/')
  sysconfdir = join_paths(prefix, sysconfdir)
endif

# required attributes
cc.compiles('''#if !__has_attribute(cleanup)
#error attribute cleanup not supported
#endif''', name: '__attribute__(cleanup)', required: true)

# required dependencies
libm = cc.find_library('m', required: false)
girara = dependency('girara', version: '>=2026.07.07')
xkbcommon = dependency('xkbcommon')
glib = dependency('glib-2.0', version: '>=2.84')
gio = dependency('gio-unix-2.0', version: '>=2.84', required: host_machine.system() != 'windows')
gthread = dependency('gthread-2.0', version: '>=2.84')
gmodule = dependency('gmodule-no-export-2.0', version: '>=2.84')
gtk4 = dependency('gtk4', version: '>=4.12', include_type: 'system')
json_glib = dependency('json-glib-1.0')
cairo = dependency('cairo')
magic = dependency('libmagic')
sqlite = dependency('sqlite3', version: '>=3.35.0')
xxhash = dependency('libxxhash')
archive = dependency('libarchive')

build_dependencies = [libm, girara, glib, gio, gthread, gmodule, gtk4, cairo, magic, json_glib, xxhash, xkbcommon, archive]

# defines
defines = [
  '-DGETTEXT_PACKAGE="io.github.bortoq.zatura"',
  '-DLOCALEDIR="@0@"'.format(join_paths(prefix, localedir)),
  '-DZATHURA_PLUGINDIR="@0@"'.format(join_paths(prefix, plugindir)),
  '-DZATHURA_LEGACY_PLUGINDIR="@0@"'.format(join_paths(prefix, get_option('libdir'), 'zathura')),
  '-DSYSCONFDIR="@0@"'.format(sysconfdir),
  '-D_DEFAULT_SOURCE',
]
if get_option('manpages').allowed()
  defines += '-DWITH_MANPAGES'
endif

# compile flags
flags = [
  '-Werror=implicit-function-declaration',
  '-Werror=vla',
  '-Werror=int-conversion',
  '-Werror=maybe-uninitialized'
]
flags = cc.get_supported_arguments(flags)

# optional dependencies
synctex = dependency('synctex', version: '>=2', required: get_option('synctex'))
seccomp = dependency('libseccomp', required: get_option('seccomp'))
landlock = cc.check_header('linux/landlock.h', required: get_option('landlock'))
if landlock
  landlock = cc.has_member('struct landlock_ruleset_attr', 'scoped', args: ['-D_GNU_SOURCE'], prefix: ['#include <linux/landlock.h>'])
endif
if get_option('landlock').enabled() and not landlock
  error('Requested Landlock sandbox needs headers with scoped ruleset support')
endif
if landlock and not seccomp.found()
  if get_option('landlock').enabled()
    error('Landlock sandbox requires seccomp for filesystem metadata restrictions')
  endif
  message('Disabling automatic Landlock sandbox: seccomp is unavailable')
  landlock = false
endif

if synctex.found()
  build_dependencies += synctex
  defines += '-DWITH_SYNCTEX'
endif

# generate version header file
version_header = configure_file(
  input: 'zatura/version.h.in',
  output: 'zathura-version.h',
  configuration: conf_data
)

include_directories = [
  include_directories('.')
]

subdir('data')
subdir('po')

# source files
sources = files(
  'girara-gtk/callbacks.c',
  'girara-gtk/commands.c',
  'girara-gtk/completion.c',
  'girara-gtk/config.c',
  'girara-gtk/dialog.c',
  'girara-gtk/inputbar.c',
  'girara-gtk/keycodes.c',
  'girara-gtk/notification-area.c',
  'girara-gtk/session.c',
  'girara-gtk/settings.c',
  'girara-gtk/shortcuts.c',
  'girara-gtk/statusbar.c',
  'girara-gtk/utils.c',
  'zatura/adjustment.c',
  'zatura/bookmarks.c',
  'zatura/callbacks.c',
  'zatura/commands.c',
  'zatura/completion.c',
  'zatura/config.c',
  'zatura/content-type.c',
  'zatura/database.c',
  'zatura/database-null.c',
  'zatura/dbus-interface.c',
  'zatura/document.c',
  'zatura/document-widget.c',
  'zatura/file-monitor.c',
  'zatura/file-monitor-glib.c',
  'zatura/file-monitor-noop.c',
  'zatura/file-monitor-signal.c',
  'zatura/index-element-object.c',
  'zatura/jumplist.c',
  'zatura/links.c',
  'zatura/marks.c',
  'zatura/page.c',
  'zatura/page-effects.c',
  'zatura/page-widget.c',
  'zatura/plugin.c',
  'zatura/print.c',
  'zatura/render.c',
  'zatura/shortcuts.c',
  'zatura/synctex.c',
  'zatura/types.c',
  'zatura/utils.c',
  'zatura/zatura.c',
  'zatura/view-settings.c',
)
sources += zatura_resources

# header files to install
headers = files(
  'zatura/document.h',
  'zatura/links.h',
  'zatura/macros.h',
  'zatura/page.h',
  'zatura/plugin-api.h',
  'zatura/reflow.h',
  'zatura/types.h',
)
headers += version_header

# zatura helper library
libzatura = static_library('zatura',
  sources + ['zatura/database-sqlite.c'],
  dependencies: build_dependencies + [sqlite],
  include_directories: include_directories,
  c_args: defines + flags,
  gnu_symbol_visibility: 'hidden',
)
libzatura_dep = declare_dependency(link_with: libzatura)

# zatura-sandbox executable
if seccomp.found() or landlock or target_machine.system() == 'openbsd'
  sandbox_sources = files()
  if target_machine.system() == 'linux'
    sandbox_sources += files('zatura/sandbox-fds.c')
  endif
  sandbox_defines = ['-DWITH_SANDBOX']
  sandbox_dependencies = []
  if seccomp.found()
    sandbox_defines += '-DWITH_SECCOMP'
    sandbox_dependencies += seccomp
    sandbox_sources += files('zatura/seccomp-filters.c', 'zatura/seccomp-open.c')
  endif
  if landlock
    sandbox_defines += '-DWITH_LANDLOCK'
    sandbox_sources += files('zatura/landlock.c')
  endif

  # zatura helper library
  libzatura_sandbox = static_library('zatura-sandbox',
    sources + sandbox_sources,
    dependencies: build_dependencies + sandbox_dependencies,
    include_directories: include_directories,
    c_args: defines + flags + sandbox_defines,
    gnu_symbol_visibility: 'hidden',
  )
  libzatura_sandbox_dep = declare_dependency(link_with: libzatura_sandbox)

  zatura_sandbox = executable(
    'zatura-sandbox',
    files('zatura/main.c'),
    dependencies: build_dependencies + [libzatura_sandbox_dep] + sandbox_dependencies,
    install: true,
    include_directories: include_directories,
    c_args: defines + sandbox_defines + flags,
    gnu_symbol_visibility: 'hidden',
    export_dynamic: true,
    win_subsystem: 'windows'
  )
endif

# zatura executable
zatura = executable(
  'zatura',
  files('zatura/main.c'),
  dependencies: build_dependencies + [libzatura_dep],
  install: true,
  include_directories: include_directories,
  c_args: defines + flags,
  gnu_symbol_visibility: 'hidden',
  export_dynamic: true,
  win_subsystem: 'windows'
)
install_headers(headers, subdir: 'zatura')
install_headers(headers, subdir: 'zathura')

# pkg-config file
pkg = import('pkgconfig')
pkg.generate(
  name: 'zatura',
  description: 'document viewer - plugin API',
  url: 'https://github.com/bortoq/zatura',
  version: version,
  requires: ['girara', 'cairo'],
  variables: [
    'apiversion=@0@'.format(plugin_api_version),
    'abiversion=@0@'.format(plugin_abi_version),
    'plugindir=${libdir}/zatura'
  ]
)
pkg.generate(
  name: 'zathura',
  description: 'document viewer - compatible plugin API',
  version: version,
  requires: ['girara', 'cairo'],
  variables: [
    'apiversion=@0@'.format(plugin_api_version),
    'abiversion=@0@'.format(plugin_abi_version),
    'plugindir=${libdir}/zathura'
  ]
)

zatura_dependency = declare_dependency(link_with: zatura, include_directories: include_directories)

if get_option('manpages').allowed()
  subdir('doc')
endif
subdir('tests')
