# Zatura: fullscreen cleanup and girara removal

## Fullscreen and normal mode cleanup

Fullscreen is now a GTK window state. F11 and `toggle_fullscreen` change the
window state without changing the active Girara shortcut mode. F11 is bound
in normal, index, insert, presentation, and inputbar contexts. The former
fullscreen mode and its duplicate key and mouse tables have been removed.
Presentation remains a separate mode because it also changes page layout,
zoom, and bar visibility. Leaving presentation restores the window state
from before it started.

Use `--fullscreen` to start with a fullscreen window, with or without a
document. The old `--mode fullscreen` spelling is read as an alias for this
flag. Existing `map [fullscreen]` and `unmap [fullscreen]` configuration
entries are read as `[normal]` with a deprecation warning. There was no
persisted fullscreen shortcut mode in the document database to migrate.

## Girara removal estimate

The dependency is deeply integrated: roughly 1,600 `girara_` references in
`zatura/`, across 49 source files, plus the local `girara-gtk/` layer.
`zatura/config.c` alone registers about 150 shortcuts, 100 settings, and
dozens of command and argument mappings. Replacing the package requires
reimplementing behavior, not just deleting an include or Meson dependency.

| Work | Main replacement | Estimate |
| --- | --- | ---: |
| Core types, lists, trees, log, paths | GLib containers and small helpers | 1-2 weeks |
| Settings and configuration | typed settings, parser, migration and callbacks | 2-3 weeks |
| Key/mouse bindings and modes | GTK controllers plus the physical keycode layer | 2-3 weeks |
| Command bar, history, completion, notifications | local GTK widgets and command registry | 2-3 weeks |
| Session/window integration and regression testing | local session model, X11/Wayland testing | 2-4 weeks |

Total: about 9-15 engineer-weeks for a functional replacement, plus plugin
compatibility and release work. The estimate assumes one experienced C/GTK
developer and excludes a full UI redesign. Risk is highest in configuration
compatibility and the plugin ABI. A practical sequence is to first replace
girara types behind adapters, then move configuration and bindings, then
remove the remaining `girara-gtk` layer and the `girara` Meson dependency.

## Minimal plugin compatibility path

The public C API now uses the original `zathura_*` symbols, `ZATHURA_*`
macros, and `zathura_plugin` definition again. The executable, application
ID, UI text, configuration directory, and repository keep the `zatura` name.
Plugin API and ABI versions remain 8 and 9; `plugin-api.h` matches upstream
at this version. Both `<zathura/...>` and `<zatura/...>` header locations are
installed, together with `zathura.pc` and `zatura.pc`. The loader searches
the original `libdir/zathura` and the newer `libdir/zatura` plugin directories
and accepts both `ZATHURA_PLUGINS_PATH` and `ZATURA_PLUGINS_PATH`.

The unmodified upstream `zathura-pdf-poppler` source builds against
`zathura.pc`, installs into `libdir/zathura`, loads in Zatura, and opens a PDF
in the GTK window. This checks the real plugin build and load path. Binary
compatibility with every previously built plugin and every earlier release
still needs separate testing. For installation beside upstream Zathura, use a
separate prefix: the compatibility headers and pkg-config file have the same
paths as upstream's files.
