# Dependency sources

Zatura and modified engines are in this repository. Original engine revisions:
[plugins/README.md](../plugins/README.md). Girara utility core is linked statically
from [e120cc0c](https://github.com/pwmt/girara/tree/e120cc0c4f486af935f09a07420606925a6f9305);
its license notice accompanies the core packages.

Native packages use distribution libraries. GNOME Flatpak uses the shared
Platform 49; platform sources are maintained by
[GNOME](https://gitlab.gnome.org/GNOME/gnome-build-meta).

AppImage contains unmodified Debian 13 shared libraries selected by linuxdeploy.
`usr/share/licenses/zatura/dependencies` contains library copyright notices;
`build-packages.tsv` records binary/source package versions (including build
packages that are not shipped). Debian provides corresponding source tarballs,
patches and build recipes through [Debian Sources](https://sources.debian.org/)
and [Debian Snapshot](https://snapshot.debian.org/). Use the recorded source
package and version, or enable matching `deb-src` repositories and run
`apt-get source PACKAGE=VERSION`. Libraries remain dynamically linked and
replaceable under `usr/lib` in an extracted AppImage.

Older 2026.10.03.1 bundles used Alpine 3.24 and a private musl runtime; see the
[SOURCES.md at that tag](https://github.com/bortoq/zatura/blob/v2026.10.03.1/doc/SOURCES.md).
