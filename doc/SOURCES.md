# Sources used by the portable release

Zatura and its modified plugins: the release source archive and the
[v2026.10.03 tag](https://github.com/bortoq/zatura/tree/v2026.10.03).
Original plugin authors and revisions are listed in [plugins/README.md](../plugins/README.md).

The source archive's `external-sources/` also contains these unmodified trees:

* Girara: `e120cc0c4f486af935f09a07420606925a6f9305`, from
  [pwmt/girara](https://github.com/pwmt/girara/tree/e120cc0c4f486af935f09a07420606925a6f9305).
* PDF/Poppler plugin: `165f37248f235bb192d154b06094c52ca92f5b2d`, from
  [pwmt/zathura-pdf-poppler](https://github.com/pwmt/zathura-pdf-poppler/tree/165f37248f235bb192d154b06094c52ca92f5b2d).
* GdkPixbuf 2.44.7: [original source archive](https://download.gnome.org/sources/gdk-pixbuf/2.44/gdk-pixbuf-2.44.7.tar.xz).
  It is built with builtin image loaders and `glycin=disabled`; see
  `tools/build-runtime-pixbuf.sh` and BUILD-MANIFEST.json.

Other runtime libraries are unmodified Alpine 3.24 packages. Exact package
versions are in BUILD-MANIFEST.json in each binary bundle. Their source archives,
patches, build scripts and license identifiers are available through
[Alpine aports 3.24](https://gitlab.alpinelinux.org/alpine/aports/-/tree/3.24-stable):
find the package's origin directory under `main/`, `community/` or `testing/`,
then use the APKBUILD matching its version. Alpine retains distfiles at
[distfiles.alpinelinux.org](https://distfiles.alpinelinux.org/distfiles/).
The manifest lists build tools as well as libraries; not every package is shipped.
The runtime is dynamically linked, and constituent libraries retain their own
licenses. Copyright notices for Zatura, bundled engines, Girara and GdkPixbuf
are included under `licenses/` in the binary bundles.
