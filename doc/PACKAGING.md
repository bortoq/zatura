# Building and distributing Zatura

## Prebuilt Linux x86_64 bundles

Download a versioned AppImage or portable archive from
[GitHub Releases](https://github.com/bortoq/zatura/releases), along with SHA256SUMS.
Verify the files from the download directory with `sha256sum -c SHA256SUMS`
(checks for artifacts you did not download will report missing files).

```sh
chmod +x Zatura-2026.10.02-x86_64.AppImage
./Zatura-2026.10.02-x86_64.AppImage document.epub
```

If FUSE is unavailable, use `--appimage-extract-and-run`, or extract once:

```sh
./Zatura-2026.10.02-x86_64.AppImage --appimage-extract
./squashfs-root/AppRun document.fb2.zip
```

For the portable archive:

```sh
tar xf zatura-2026.10.02-linux-x86_64.tar.gz
./zatura-2026.10.02/AppRun document.pdf
```

Both formats contain the same private musl runtime and five document engines:
PDF/Poppler, MuPDF (EPUB/FB2 and other supported types), DjVu, PostScript and
comic/image archives. Configuration and document history use the usual XDG
paths. Fonts and the desktop display server come from the host; no toolkit
upgrade or system install is required. The launcher regenerates image-loader
paths for its current location and keeps musl libraries out of host subprocesses.
These are dynamically linked bundles, not a fully static executable. Testing on
all distributions, Wayland compositors and sandbox configurations is pending.

## Native source builds

Use a compiler with the C23 features required by the sources, Meson >= 1.5,
GTK >= 4.12 and GLib >= 2.84; see [README](../README.md) for the remaining
libraries and optional documentation tools.

```sh
make configure PREFIX=/opt/zatura MESON_ARGS='-Dbuildtype=release'
make build JOBS=4
make test
make install
make plugins RUNTIME=/opt/zatura
```

`make` runs the build. `make help` lists the targets. Existing build directories
keep their original prefix; use `make configure MESON_ARGS='--prefix=/new/prefix'`
to reconfigure one explicitly. `DESTDIR=/tmp/package-root make install` stages
installed files without changing the build prefix. Plugin building uses
`tools/build-plugins.sh`; inspect [PLUGINS.md](PLUGINS.md) for its prerequisites.

## Bundle an installed runtime

The packaging helper expects a complete x86_64 musl prefix with the application,
plugins, runtime libraries, GSettings schemas, image loaders, MIME data,
libmagic database and Ghostscript resources. It rejects an incomplete runtime
and absolute symlinks. It does not assemble dependencies or convert a glibc build.

```sh
make portable RUNTIME=build/runtime DISTDIR=dist
APPIMAGE_EXTRACT_AND_RUN=1 make appimage RUNTIME=build/runtime \
    APPIMAGETOOL=/path/to/appimagetool-x86_64.AppImage
```

Use the official [appimagetool](https://github.com/AppImage/appimagetool).
Its build and the AppImage runtime versions should be pinned in a publishing
pipeline. The initial release records runtime package versions in
BUILD-MANIFEST.json. `dist/Zatura.AppDir` is generated output; the packaging
helper replaces it on the next invocation. `make clean` cleans compiled objects;
it does not remove document history or configuration.

AppImage and archive are the initial distribution formats. deb, AUR and Flatpak
recipes, reproducible container builds and automated releases are scheduled in
[REFACTORING.md](REFACTORING.md). Do not install the compatibility headers and
pkg-config files into an upstream Zathura prefix without checking file conflicts.

## Release sources

`make source` archives the committed project, so commit changes before publishing.
The first release's source archive additionally includes the exact Girara,
upstream PDF/Poppler plugin and GdkPixbuf trees used for its build:

```sh
make source SOURCE_ARGS='--extra-source girara=subprojects/girara \
    --extra-source pdf-poppler=/path/to/zathura-pdf-poppler \
    --extra-source gdk-pixbuf=/path/to/gdk-pixbuf-2.44.7'
```

Zatura's modified engines live in `plugins/` with their original notices.
The runtime manifest gives the installed Alpine package versions and the
AppImage tool/runtime hashes. See [SOURCES.md](SOURCES.md) for dependency sources.
