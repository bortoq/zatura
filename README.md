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

[Releases](https://github.com/bortoq/zatura/releases) provide Linux x86_64 AppImage and portable bundles.

```sh
chmod +x Zatura-*.AppImage
./Zatura-2026.10.03.1-x86_64.AppImage document.epub
# Without FUSE: add --appimage-extract-and-run.
```

Native builds require a C23 compiler, Meson >= 1.5, GTK >= 4.12, GLib >= 2.84,
Girara >= 2026.07.07, Cairo, libxkbcommon, libmagic, JSON-GLib, SQLite >= 3.35,
xxhash, libarchive, gettext and pkgconf. SyncTeX and seccomp are optional.
Sphinx builds manuals; Doxygen/Breathe build API documentation.

```sh
make configure PREFIX=/opt/zatura MESON_ARGS='-Dbuildtype=release'
make build
make test
make install
make plugins RUNTIME=/opt/zatura
```

Keep a separate prefix from Zathura because compatibility headers share names.
`DESTDIR=/tmp/package-root make install` stages an installation.

Configuration: `~/.config/zatura/zaturarc` (or `$XDG_CONFIG_HOME/zatura/zaturarc`).
See the [reading/configuration guide](doc/READING.md), [build and packaging notes](doc/PACKAGING.md),
[remaining work](doc/REFACTORING.md) and [changelog](CHANGELOG.md).
[Issues](https://github.com/bortoq/zatura/issues) · [Discussions](https://github.com/bortoq/zatura/discussions).
