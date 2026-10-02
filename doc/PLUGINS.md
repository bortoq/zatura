# Document plugins in Zatura

The local `zz` build includes these plugins:

| Backend | Formats |
| --- | --- |
| Poppler | PDF |
| MuPDF, with PDF disabled | EPUB, FB2 (also inside ZIP), MOBI, XPS/OXPS, XHTML, SVG, JPEG, PNG, BMP, TIFF |
| DjVuLibre | DjVu, including multiple pages |
| libspectre / Ghostscript | PostScript, EPS |
| libarchive / GdkPixbuf | CBZ, CBR, CB7, CBT; ZIP, RAR, 7z and TAR containing images; directories of images |

MOBI is advertised by the upstream plugin; this build has not been checked
with a MOBI sample. Archive readers depend on the compression methods supported
by libarchive. Encrypted books and archive password input are not implemented.

Sources are vendored in `plugins/`, with exact upstream revisions in
`plugins/README.md` and original licenses retained. The original Zathura plugin
API remains compatible; the application is `zatura`.

## Build

First compile and install Zatura and Girara into a separate prefix.
Install development dependencies for MuPDF >=1.26, DjVuLibre, libspectre,
libarchive and GTK4, then run:

```sh
./tools/build-plugins.sh /absolute/path/to/prefix
```

MuPDF's PDF registration is disabled to retain the installed Poppler plugin.
For the existing Alpine runtime, `tools/Dockerfile.plugins` adds the required
libraries to `zatura-build:local`. The runtime also needs their shared-library
dependencies, MIME database and Ghostscript resources. The native wrapper sets
`GS_LIB` and `GS_FONTPATH` to resources inside the runtime.

The local runtime uses GdkPixbuf 2.44.7 with built-in image loaders; source:
https://download.gnome.org/sources/gdk-pixbuf/2.44/gdk-pixbuf-2.44.7.tar.xz
`tools/build-runtime-pixbuf.sh` builds it into a given runtime prefix, avoiding
external helper executables for comic image decoding. This affects only the
private runtime, not system GTK or GLib.

EPUB styles can be configured in `~/.config/zatura/epub.css` (the legacy
`~/.config/zathura/epub.css` is used when the new file is absent).

## Rendering verification

```sh
python3 tests/make_format_fixtures.py /tmp/zatura-format-fixtures
```

`build/tests/test_plugin_formats` accepts fixture paths, opens each document
through normal MIME selection and verifies every page renders nonempty pixels.
Run it with Xvfb or a display and the same runtime environment as `zz`.

Verified outside the build container: FB2 with Russian text, EPUB with two
chapters, DjVu, PostScript with font rendering, XPS, PNG, SVG, CBZ and CBT.
The existing PDF integration test verifies display effects and continuous
adjustments. CBR, CB7, MOBI, XHTML, OXPS, JPEG, BMP, TIFF and EPS registration is
present; those variants were not separately tested with sample documents.

## FB2 archives

ZIPs containing an FB2 member are routed to MuPDF before generic ZIP comic
handling. `.fb2.zip`, ordinary `.zip`, and `.fb2z` files are supported, including
uppercase extensions and nested Unicode member names. If several books are
present, the first FB2 member in archive order is opened. Images and other files
before the book are ignored. ZIP members are streamed into MuPDF without disk
extraction. The original ZIP path remains the document path for history, file
monitoring and reload. Archive header inspection is limited to 4096 entries.

Verified ZIP fixtures include multiple books, nested Russian member names and
ZIP Deflate compression. Plain FB2, EPUB and ZIP comic loading still pass.

## Book size and font controls

Reflowable books automatically adapt their page dimensions to the viewport
and `pages-per-row`. In a two-page spread, each page takes half the available
width and fits the viewport height. Resizing the window repaginates the book.

`Ctrl+-` decreases the default text size; `Ctrl++` or `Ctrl+=` increases it.
The numeric keypad works too, independently of keyboard layout. The setting
is `reflow-font-size` (default 12 pt, range 6-72 pt). These controls change
book pagination and preserve the current page's content location. They do
not alter fixed-layout PDF, images or DjVu. The controls change MuPDF's
default font size; explicitly fixed font sizes in publisher CSS may behave
differently from relative font sizes.
