zatura - a document viewer
===========================

zatura is a highly customizable and functional document viewer based on the
girara user interface library and several document libraries.
It is a fork of [zathura](https://github.com/pwmt/zathura).

Differences from Zathura
-----------------------

* Physical keyboard shortcuts work identically with English and Russian layouts.
* Normal and fullscreen windows share the same shortcuts; F11 changes the window state.
* Contrast, brightness, gamma and saturation controls use keys 1–8, with responsive key repeat.
* EPUB and FB2 books (including FB2 in ZIP) fit one or two columns to the window;
  Ctrl+- / Ctrl++ changes the text size, and four configurable page margins save screen space.
* Document history also restores image adjustments, book layout and viewing preferences.
* `t` toggles a local HH:MM clock before the page number.
* The original Zathura plugin API remains supported; bundled plugin sources cover
  books, PDF, DjVu, PostScript, images and comic archives.

Configuration is `~/.config/zatura/zaturarc`. See the [complete annotated example](doc/zaturarc.example),
[English reading guide](doc/READING.md), [русское руководство](doc/READING.ru.md), [plugin/format details](doc/PLUGINS.md) and
[refactoring notes](doc/REFACTORING.md).

Requirements
------------

The following dependencies are required:

* `gtk4` (>= 4.12)
* `glib` (>= 2.84)
* `girara` (>= 2026.07.07)
* `libxkbcommon` (for layout independent shortcuts)
* `libmagic` from file(1): for mime-type detection
* `json-glib`
* `sqlite3` (>= 3.35.0): sqlite3 database backend
* `libxxhash`: file hashing
* `libarchive`: archive detection and FB2 ZIP handling

The following dependencies are optional:
* `libsynctex` from TeXLive (>= 2): SyncTeX support
* `libseccomp`: sandbox support

For building zatura, the following dependencies are also required:

* `meson` (>= 1.5)
* `gettext`
* `pkgconf`

The following dependencies are optional build-time only dependencies:

* `librsvg-bin`: PNG icons
* `Sphinx`: manpages and HTML documentation
* `doxygen`: HTML documentation
* `breathe`: for HTML documentation
* `sphinx_rtd_theme`: for HTML documentation

Note that `Sphinx` is needed to build the manpages. If it is not installed, the
man pages won't be built. For building the HTML documentation, `doxygen`,
`breathe` and `sphinx_rtd_theme` are needed in addition to `Sphinx`.

The use of `libseccomp` and/or `landlock` to create a sandboxed environment is
optional and can be disabled by configure the build system with
`-Dseccomp=disabled` and `-Dlandlock=disabled`. The sandboxed version of zatura
will be built into a separate binary named `zatura-sandbox`.  Strict sandbox
mode will reduce the available functionality of zatura and provide a read only
document viewer.

Installation
------------

Download the AppImage or portable Linux x86_64 archive from
[Releases](https://github.com/bortoq/zatura/releases). Both include the document
engines. See [packaging instructions](doc/PACKAGING.md) and [changelog](CHANGELOG.md).

With a C23 compiler and the dependencies above, build through the Makefile:

```sh
make configure PREFIX=/usr/local
make build
make test
make install
```

Meson remains the build system; `MESON_ARGS` passes its configuration options.
`make plugins RUNTIME=/your/private/prefix` builds bundled engines after installing
Zatura into that prefix. Keep a separate prefix when installing alongside Zathura:
compatibility headers and pkg-config files share upstream names.
`DESTDIR=/tmp/package-root make install` stages files for distribution packages.
A complete musl runtime can be packed with `make portable` or `make appimage`;
these packaging targets do not construct that runtime from a native glibc build.


Bugs
----

Please report bugs in [Issues](https://github.com/bortoq/zatura/issues).
Questions and feature ideas are welcome in [Discussions](https://github.com/bortoq/zatura/discussions).
