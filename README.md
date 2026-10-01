zatura - a document viewer
===========================

zatura is a highly customizable and functional document viewer based on the
girara user interface library and several document libraries.
It is a fork of [zathura](https://github.com/pwmt/zathura).

Requirements
------------

The following dependencies are required:

* `gtk4` (>= 4.12)
* `glib` (>= 2.84)
* `girara` (>= 2026.07.07)
* `libxkbcommon` (for layout independent shortcuts)
* `libmagic` from file(1): for mime-type detection
* `json-glib`
* `sqlite3` (>= 3.25.0): sqlite3 database backend
* `libxxhash`: file hashing

The following dependencies are optional:
* `libsynctex` from TeXLive (>= 2): SyncTeX support
* `libseccomp`: sandbox support

For building zatura, the following dependencies are also required:

* `meson` (>= 1.5)
* `gettext`
* `pkgconf`

The following dependencies are optional build-time only dependencies:

* `librvsg-bin`: PNG icons
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

To build and install zatura using meson's ninja backend:

    meson build
    cd build
    ninja
    ninja install

> **Note:** The default backend for meson might vary based on the platform. Please
refer to the meson documentation for platform specific dependencies.

Bugs
----

Please report bugs at https://github.com/bortoq/zatura/issues.
