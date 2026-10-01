# Zatura: removing fullscreen mode and girara

## Fullscreen and normal mode cleanup

Keep fullscreen as a GTK window state, controlled by F11 and the existing
`toggle_fullscreen` command. It must not change the active shortcut mode.
This retains a useful window feature while making document interaction the
same in both window states.

1. In `zatura/config.c`, stop registering the `fullscreen` girara mode. Delete
   `FULLSCREEN`, the second call to `add_default_shortcuts`, the second call to
   `add_default_mouse_events`, and the fullscreen F11 binding. Register F11
   once in normal mode. Remove fullscreen from `all_modes`.
2. In `zatura/shortcuts.c`, change `sc_toggle_fullscreen` to toggle the GTK
   window state only. Handle F11 while the index or insert view is active if
   fullscreen must be available there as well. Keep presentation separate:
   it changes zoom, page layout, and bar visibility in addition to window
   state. Review its exit path so it restores the previous window state.
3. Remove `modes.fullscreen` from `zatura/zatura.h`. In
   `zatura/callbacks.c`, permit selection based on document view context,
   without checking a fullscreen mode. Audit all `girara_mode_get` branches
   for assumptions that fullscreen has a unique shortcut table.
4. Replace `--mode fullscreen` and restored document mode `fullscreen` with
   an initial window-state flag. Migrate saved `fullscreen` values on read;
   write only the new representation. Update completions, manual pages,
   configuration documentation, and tests. If old user configuration uses
   `[fullscreen]`, accept it as a deprecated alias for `[normal]` for one
   transition period, then remove the alias.
5. Test F11 before and after opening a document, in normal/index/insert
   views, after presentation, across restored sessions, and under both X11
   and Wayland. Check that every normal shortcut, mouse action, selection,
   and command behaves identically in fullscreen.

This removes a whole mode, one duplicate default binding table, special case
branches, and persisted mode semantics. It does not remove index, insert,
inputbar, or presentation behavior; those have different purposes and need
separate product decisions.

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
