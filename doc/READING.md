# Reading with Zatura

[Русская версия](READING.ru.md)

## Installation and configuration

Download the Linux x86_64 AppImage or portable archive from
[Releases](https://github.com/bortoq/zatura/releases). See [PACKAGING.md](PACKAGING.md)
for launching, building and packaging instructions. Document engines are included
in these bundles; native builds need the appropriate plugins.

The configuration file is `~/.config/zatura/zaturarc`, or
`$XDG_CONFIG_HOME/zatura/zaturarc`. The [annotated example](zaturarc.example)
lists all settings and default bindings (its explanatory comments are Russian).
The [configuration manual](man/zaturarc.5.rst) describes the settings in English.
Use `:set name value` to apply a change immediately. Press Tab after `:set `
to complete settings that can be changed at runtime, including image controls
and book margins.
Persistent defaults belong in the configuration file. `:source` reloads a file;
settings marked as available only at startup require restarting the application.

## Navigation and keyboard layouts

Open a file with `zatura path/to/document`. `j` / `k` and the mouse wheel scroll;
Space / Shift+Space move forward / backward. `/` starts text search, `n` / `N`
visit its next / previous match. Tab opens the table of contents where supported.
`+` / `-` change zoom, `a` fits the page, and `s` fits its width.
`F11` changes the window's fullscreen state without changing reading shortcuts.
Presentation (`F5`) remains a separate layout mode. `q` quits.

Shortcut names refer to physical US keyboard positions and work with Russian
and other layouts. Text entered in search and commands uses the current layout.
User mappings override defaults, for example `map <C-0> reset_page_effects`.
The number row controls image adjustments; unmap these keys to recover the
original numeric command prefixes. See the example configuration for every
keyboard and mouse mapping.

## EPUB and FB2 books

* `d` switches one / two columns; pages automatically fit the available window.
* Space / Shift+Space advance by exact spreads without accumulating scroll drift.
* Ctrl+- reduces text size; Ctrl++ or Ctrl+= increases it.
* The wheel and `j` / `k` scroll in smaller steps.
* FB2 documents inside ZIP archives open directly.

Book layout controls:

```conf
set reflow-font-size 12
set reflow-margin-top 4
set reflow-margin-bottom 4
set reflow-margin-outer 4
set reflow-margin-inner 4
```

Font size is in points; margins are in logical GTK pixels. In a spread, `outer`
is the window edge and `inner` is the central edge. A single column uses `outer`
on both sides. Zero removes that margin. Oversized margins shrink proportionally
to leave room for text. The gap between the two text areas is
`2 × inner + page-h-padding`. First-column and right-to-left settings determine
which margin belongs to each page. Publisher paragraph spacing and styling
remain; default root-body and page margins are reset. A user stylesheet can be
placed in `~/.config/zatura/epub.css` (legacy `~/.config/zathura/epub.css` is also read).
Fixed PDF/DjVu pages and images are unaffected by book layout settings.

## Image adjustments and clock

| Decrease / increase | Control | Configuration setting |
| --- | --- | --- |
| `1` / `2` | Contrast | `page-contrast` |
| `3` / `4` | Brightness | `page-brightness` |
| `5` / `6` | Gamma | `page-gamma` |
| `7` / `8` | Saturation | `page-saturation` |

All four controls range from −100 to 100; zero is neutral. Hold a key to repeat
an adjustment. Printing and exporting the original document are unchanged.
`reset_page_effects` resets these controls while retaining recolor. Copying a
selection is quiet by default; enable `selection-notification` to request notices.
Errors and explicit information commands still report their results.

`t` toggles local time `HH:MM` before the page counter; it updates automatically.
To enable it by default and customize the toggle:

```conf
set statusbar-show-time true
map [normal] t toggle_time
map [presentation] t toggle_time
```

The clock is visible only when `guioptions` includes `s` (the status bar).

## Saving document views

```conf
set database "sqlite"
set save-view-settings true
```

Closing a document or quitting normally saves its page, position, zoom, fit mode,
rotation, number and direction of columns. It also saves image controls, recolor,
book font and margins, page spacing, single-page mode, panels and clock visibility.

These preferences are stored per document in
`~/.local/share/zatura/bookmarks.sqlite`, or `$XDG_DATA_HOME/zatura/`.
Reopening restores the document's saved values ahead of configured defaults;
new documents start from the configuration. `save-view-settings false` disables
the additional view snapshot; `database "null"` disables all document history.
A forcibly killed process cannot save changes at exit.

Numeric bookmarks and jump history are not yet tied to book content: changing
font size or margins may make them point to different text. The current reading
position is preserved during live MuPDF reflow. Stable anchors for saved bookmarks
and jumps are scheduled in [REFACTORING.md](REFACTORING.md).

## Formats, troubleshooting and feedback

See [PLUGINS.md](PLUGINS.md) for formats, dependencies and engine limitations.
If a native build cannot open a format, check that its plugin is installed and
compatible with plugin API 8 / ABI 9. Bundles keep engines in their private runtime.

Report reproducible problems in [Issues](https://github.com/bortoq/zatura/issues),
including Zatura version, document format, desktop/display system, relevant
configuration and reproduction steps. Use [Discussions](https://github.com/bortoq/zatura/discussions)
for questions and feature ideas. Avoid attaching private documents; a small public
or generated example is preferable.
