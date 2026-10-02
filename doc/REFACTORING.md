# Zatura refactoring plan

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

## Page brightness, contrast, gamma, and saturation

Implemented: CPU display adjustments in `zatura/page-effects.c/.h`, shared
postprocessing for asynchronous page rendering and the synchronous first page,
configuration settings, physical key bindings, and reset. The controls work
with recolor enabled or disabled and in normal/fullscreen and presentation.

| Setting | Range | Default | Decrease / increase |
| --- | --- | --- | --- |
| `page-contrast` | −100…100 | 0 | `1` / `2` |
| `page-brightness` | −100…100 | 0 | `3` / `4` |
| `page-gamma` | −100…100 | 0 | `5` / `6` |
| `page-saturation` | −100…100 | 0 | `7` / `8` |

The key pairs and step of 1 follow [mpv's default input bindings](https://github.com/mpv-player/mpv/blob/master/etc/input.conf).
Values are integers; out-of-range values are clamped with a warning. Zero for
all four controls preserves pixels exactly. For example,
`:set page-gamma 20` lifts midtones; `:set page-saturation -100` produces grayscale.
Defaults can be stored in `zaturarc`; interactive changes last for the session.

The number-row bindings take priority over the old numeric command prefixes.
Unmap the individual number keys to restore their prefix behavior. Text entry
in the command bar is unaffected, and user mappings can override these defaults.
Shortcut actions `adjust_brightness`, `adjust_contrast`, `adjust_gamma`, and
`adjust_saturation` accept `up` / `down` and use the existing count mechanism.
The `reset_page_effects` action resets all four adjustments, retaining recolor.
For example, add `map <C-0> reset_page_effects` to `zaturarc`.

### Rendering and cache correctness

The CPU path works with the local launcher's Cairo renderer. The sequence is
original plugin rendering, optional recolor, saturation, contrast plus additive
brightness, then gamma. Adjustments apply to the entire page, including images
excluded from recolor by `recolor-reverse-video`. Search/selection highlights
and link markers are drawn afterward with their normal UI colors.

For normalized encoded RGB channels, define `s = 1 + saturation/100`,
`k = 1 + contrast/100`, `b = brightness/100`, and
`Y = 0.2126 R + 0.7152 G + 0.0722 B`. Each channel is transformed as
`V = clamp(0.5 + k * (Y + s * (C - Y) - 0.5) + b, 0, 1)`, then
`Cout = V ** (2 ** (-gamma/50))`.
Gamma uses a 4097-entry lookup table to avoid a power calculation for every
pixel channel. Positive gamma lifts midtones; negative gamma darkens them,
with black and white endpoints preserved when only gamma is adjusted.
The control curve is defined here; matching mpv's keys does not promise
pixel-identical output to its video renderers. This is a display adjustment
on encoded RGB, not color-managed linear-light exposure correction.

The filter handles native-endian Cairo RGB24 and premultiplied ARGB32, stride,
transparent pixels, flush, and dirty marking. It preserves alpha by adjusting
unpremultiplied colors and premultiplying the result. Neutral values skip the
filter entirely. Adjustments always begin from immutable original pixels, reused from the raw cache when available.

`PageEffects` is independent of Girara; only the settings adapter uses Girara.
Each worker snapshots the latest settings when it starts processing. Setting
changes mark processed images obsolete without clearing the displayed surface.
An active adjustment completes and presents its intermediate frame even if
newer settings arrive during filtering. Only one job per page request can be
active, including a completion waiting for the main thread. After presenting,
a changed generation schedules one more job with the latest settings. This
prevents keyboard repeat from continuously canceling every visible update.
The widget records the generation actually delivered, rather than the current
settings generation. Explicit cancellation for zoom, visibility, or document
closure still rejects invalid frames; filters check it every 64 rows.
GTK redraws coalesce changes made before the next frame. Hidden pages refresh
when shown. No layout calculation runs on the adjustment path.

Original opaque ARGB32 surfaces are stored in a renderer-owned LRU cache with
an additional 128 MiB limit. The key includes page identity, raster dimensions,
render scale, and device scale. This cache is freed with the document renderer;
zoom/HiDPI changes miss the key. Large pages exceeding the limit still render
correctly but cannot be reused. Filtering modifies a copy, never the cached
original. Neutral settings share the immutable original without a pixel copy.
Existing processed-image caching is separate; the 128 MiB limit applies to the
additional original-pixel storage, not total application memory. Opaque pixels
without saturation mixing and grayscale pixels use byte lookup tables. Local
builds now use release optimization.

The `render_plain` path bypasses display effects. Printing calls the plugin's
page renderer directly; attachment export and original image export/clipboard
also bypass the display filter. The plugin API and ABI are unchanged.

### Remaining refactoring

1. Extract the existing recolor implementation from `render.c` into the local
   effects module; both rendering paths already share postprocessing.
2. Add a compact panel with four sliders, values, and reset; debounce slider
   changes. Estimate: 2–4 engineer-days.
3. Measure adjustment latency and total cache memory on large vector and scanned
   pages, including zoom/HiDPI and recolor. The bounded original-pixel cache is
   implemented; consider a shared budget with processed surfaces if measured
   memory consumption warrants it.
4. Consider per-document persistence and later GPU acceleration after moving
   page drawing to textures, retaining the CPU path for compatibility.
5. Migrate the settings adapter when removing Girara; the effect model already
   belongs to Zatura.

Pixel fixtures cover neutral identity, grayscale, brightness clipping, contrast,
gamma direction, combined filter order, alpha, and padded stride. Session tests
exercise all eight physical keys with alternate layout symbols, presentation,
bounds clamping, renderer settings, reset, and command-bar isolation.


PDF integration checks additionally count plugin render calls: cached adjustments
must not rerender the document, while changed zoom must. They verify retained
surfaces/geometry, original plain rendering, convergence after bursts of changes,
and multiple intermediate frames during continuous keyboard repeat.
The PDF integration test is skipped when no compatible PDF plugin is installed.

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


## Additional document formats

Completed: MuPDF for EPUB/FB2/XPS and images, DjVuLibre for DjVu,
libspectre/Ghostscript for PostScript/EPS, and libarchive for comic archives.
Poppler retains PDF rendering. Plugin sources and exact revisions are in
`plugins/`; the compatibility API is unchanged. See `PLUGINS.md` for builds,
format coverage, and the native runtime checks.


## Quiet display adjustment shortcuts

Brightness, contrast, gamma and saturation shortcuts, including reset, apply
without opening Girara's notification bar. Other command notifications retain
their usual behavior.

## Static build feasibility

The current build is dynamic with a private musl runtime. The build image does
not provide static GTK4 or GLib libraries. A fully static executable would
require rebuilding the dependency stack and replacing `.so` plugin discovery
with compiled-in plugin registration: musl's static `dlopen` implementation
fails by design. GTK/GIO modules would also need auditing and embedding or
disabling. MIME databases, schemas, fonts and Ghostscript resources would
still need packaging. A portable directory or AppImage around the existing
private runtime preserves dynamic plugins with much less implementation work.
Static packaging has been assessed; no static build has been produced.


## Reflowable book layout

Implemented optional plugin symbol `zatura_reflow_v1` without changing the
upstream plugin function table or ABI version. The MuPDF implementation
uses `fz_is_document_reflowable`, `fz_layout_document`, and content bookmarks
to keep the current reading location through repagination. PDF, scanned
images, DjVu and other fixed pages do not participate.

EPUB/FB2 (including ZIP), XHTML and other MuPDF reflowable documents receive
page dimensions calculated from viewport size, display PPI, column count
and inter-page spacing. Two columns each receive half the available width;
page height fits the viewport. Window resizing, column switching and
`reflow-font-size` changes schedule a layout. Changes are coalesced for at
most 50 ms without restarting the timer during key repeat.

`Ctrl+-` lowers the default font size by one point, `Ctrl++`/`Ctrl+=` raises
it; keypad variants also work. Limits are 6-72 pt, default 12. Existing
physical-key matching is preserved, including Ctrl+Shift+plus. Configuration
uses `set reflow-font-size 14`; command mappings use `adjust_book_font`.

Before layout, the renderer stops and page widgets and page objects are
released. Afterward they are recreated with the new page count; original
and processed pixel caches are cleared with the old renderer. Index links
and search rectangles are rebuilt. GTK allocation protects the mapped
page anchor until the new scroll bounds exist.

Regression checks cover long FB2, multi-chapter EPUB, FB2 in ZIP, two-page
viewport fit, resizing, font range, physical Ctrl bindings, current-page
preservation and leaving PDF unchanged. Content bookmarks preserve the
current page's text location; existing saved numeric bookmarks and jump
history do not yet store content anchors and may refer to different text
after repagination. Book layout is now persisted. Moving saved numeric bookmarks and jump
history to content anchors remains a refactoring item.

### Exact reflow height and full configuration reference

Reflow pages use the entire viewport height, without the previous eight-pixel
reserve. Full up/down scrolling of reflowable documents moves to the exact
start of the next/previous row, including the first-page column offset and
page spacing; it no longer accumulates the difference between window height
and row height. Regression checks perform repeated forward and backward
spread turns for FB2, EPUB and FB2 ZIP.

`doc/zaturarc.example` contains all 113 compiled settings and every default
keyboard assignment, including input-bar editing, plus all built-in mouse
rules. `tools/dump-config-reference.c` exports actual setting metadata;
`tools/make-config-reference.py` combines it with source bindings and the
manual to produce the annotated file. Input-bar actions can now be mapped
with `[inputbar]`, and named GDK keys can be used in configuration.

### Configurable book margins

`reflow-margin-top`, `reflow-margin-bottom`, `reflow-margin-outer`, and
`reflow-margin-inner` default to four logical GTK pixels and accept 0–1000.
Viewport scaling converts them to points. Two-page spreads mirror inner and
outer margins according to the grid's first column and RTL direction; one
column uses the outer margin on both sides. Oversized margins are reduced
proportionally to preserve a 72-point text area.

MuPDF's default `@page` margins (3em vertically, 2em horizontally) and root
body margins are reset before parsing a book. Paragraph spacing, indentation
and other publisher/user CSS remain. Content is laid out in the reduced
text area and translated into a full-size page, keeping exact viewport
height and spread navigation. Rendering, text extraction, search, selection,
image positions, and link rectangles/destinations use the same translation.

The optional `zatura_reflow_v2` symbol carries margins without changing the
original plugin function table or the v1 extension. Older v1-only plugins
continue to support font/viewport reflow but do not expose margin control.
Tests check actual ink bounds, search-to-ink overlap, asymmetric margins,
first-column changes, RTL, single-page views, zero margins and repagination
from margin changes alone for FB2, EPUB and FB2 ZIP.

### Per-document viewing history and statusbar clock

SQLite schema version 5 adds `document_view(file, settings)` without modifying
the existing fileinfo columns. A typed JSON snapshot persists image adjustments,
recoloring, reflow font and margins, single-page mode, page spacing, grid
preferences, panels and clock. `save-view-settings` defaults to true.
New documents reset to the configured defaults; known documents restore their
snapshot. Saved reflow dimensions rebuild book pagination before validating
the saved page; later viewport changes map its content bookmark as usual.
Adjust-to-width / best-fit modes now restore as well. Close, normal exit and
reload release snapshots correctly; forced termination is outside this guarantee.

`statusbar-show-time` defaults to false; physical `t` toggles `HH:MM` before
the page counter in normal/presentation modes. A timer updates idle clocks
and is removed when disabled or destroyed. The page counter itself and window
title remain independent of the clock prefix. Tests exercise fresh application
instances, old database migration, defaults isolation, book pagination and
physical toggles.
