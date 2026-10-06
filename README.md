# Zatura

A keyboard-driven document viewer forked from [Zathura](https://github.com/pwmt/zathura).

## Changes from Zathura

- Physical shortcuts independent of the active keyboard layout.
- Shared normal/fullscreen bindings (`F11`).
- Contrast, brightness, gamma and saturation on `1`–`8`.
- EPUB/FB2/FB2 ZIP reflow, fitted spreads (`d`), text size (`Ctrl+-` / `Ctrl++`) and configurable margins.
- Saved document viewing preferences and an optional clock (`t`).
- Upstream plugin API 8 / ABI 9 compatibility.

## Install and build

[Releases](https://github.com/bortoq/zatura/releases) provide Linux packages. See [packaging](doc/PACKAGING.md) for native dependency and GNOME Platform builds.

```sh
chmod +x Zatura-*.AppImage
./Zatura-2026.10.03.2-x86_64.AppImage --version
# Without FUSE: add --appimage-extract-and-run.
```

Packages built from this tree include the compatible Poppler PDF engine (API 8 / ABI 9).
Additional formats require optional engines; older distribution plugins with
another ABI are incompatible.

Native builds require a C23 compiler, Meson >= 1.6, GTK >= 4.12, GLib >= 2.84,
Girara >= 2026.07.07, Cairo, libxkbcommon, libmagic, JSON-GLib, SQLite >= 3.35,
xxhash, libarchive, gettext and pkgconf. SyncTeX and seccomp are optional.
Sphinx builds manuals; Doxygen/Breathe build API documentation.

```sh
make configure PREFIX=/opt/zatura MESON_ARGS='-Dbuildtype=release'
make build
make test
make install
make plugins RUNTIME=/opt/zatura  # requires libpoppler-glib-dev (Debian) or poppler-glib (Arch)
```

`make plugins` builds the pinned Poppler PDF engine by default and discovers
multiarch SDK directories automatically. For all engines, install MuPDF >=1.26,
DjVuLibre, libspectre and libarchive development packages, then run
`PLUGINS="pdf-poppler pdf-mupdf djvu ps cb" make plugins RUNTIME=/opt/zatura`.
With only `PLUGINS=pdf-mupdf`, MuPDF also handles PDF.

Keep a separate prefix from Zathura because compatibility headers share names.
`DESTDIR=/tmp/package-root make install` stages an installation.

Configuration: `~/.config/zatura/zaturarc` (or `$XDG_CONFIG_HOME/zatura/zaturarc`).
See the [reading/configuration guide](doc/READING.md), [build and packaging notes](doc/PACKAGING.md),
[remaining work](doc/REFACTORING.md) and [changelog](CHANGELOG.md).
[Issues](https://github.com/bortoq/zatura/issues) · [Discussions](https://github.com/bortoq/zatura/discussions).
