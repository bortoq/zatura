#!/bin/sh
# SPDX-License-Identifier: Zlib
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${ZATURA_BUNDLE_BUILD:-/work/build}
prefix="$build_dir/runtime"
mkdir -p "$prefix/lib" "$prefix/share"
export PKG_CONFIG_PATH="$prefix/lib/pkgconfig"
export LD_LIBRARY_PATH="$prefix/lib"
cross="$root/packaging/musl-x86_64.ini"
meson setup "$build_dir/core" "$root" --cross-file "$cross" --prefix "$prefix" --libdir lib \
  -Dbuildtype=release -Dmanpages=disabled -Dtests-x11=enabled -Dtests-wayland=disabled \
  -Dseccomp=disabled -Dlandlock=disabled -Dsynctex=disabled -Dconvert-icon=disabled -Dshell-completions=disabled
meson compile -C "$build_dir/core" -j4
meson install -C "$build_dir/core"
for plugin in pdf-mupdf djvu ps cb; do
  if [ "$plugin" = pdf-mupdf ]; then option=-Dpdf=disabled; else option=; fi
  meson setup "$build_dir/plugins/$plugin" "$root/plugins/$plugin" --cross-file "$cross" --prefix "$prefix" --libdir lib -Dbuildtype=release $option
  meson compile -C "$build_dir/plugins/$plugin" -j4
  meson install -C "$build_dir/plugins/$plugin"
done
mkdir -p "$build_dir/sources"
git clone --quiet https://github.com/pwmt/zathura-pdf-poppler.git "$build_dir/sources/pdf-poppler"
git -C "$build_dir/sources/pdf-poppler" checkout --quiet 165f37248f235bb192d154b06094c52ca92f5b2d
meson setup "$build_dir/plugins/pdf-poppler" "$build_dir/sources/pdf-poppler" --cross-file "$cross" --prefix "$prefix" --libdir lib -Dbuildtype=release
meson compile -C "$build_dir/plugins/pdf-poppler" -j4
meson install -C "$build_dir/plugins/pdf-poppler"
curl --fail --location --output "$build_dir/sources/gdk-pixbuf.tar.xz" https://download.gnome.org/sources/gdk-pixbuf/2.44/gdk-pixbuf-2.44.7.tar.xz
printf '%s  %s\n' 172f80e3626ec31520a970400f1a3694e04718f6c2cd2885f75250fb5a6995a4 "$build_dir/sources/gdk-pixbuf.tar.xz" | sha256sum -c -
tar xf "$build_dir/sources/gdk-pixbuf.tar.xz" -C "$build_dir/sources"
meson setup "$build_dir/pixbuf" "$build_dir/sources/gdk-pixbuf-2.44.7" --cross-file "$cross" --prefix "$prefix" --libdir lib \
  -Dbuildtype=release -Dglycin=disabled -Dbuiltin_loaders=all -Dothers=enabled -Dpng=enabled -Djpeg=enabled -Dtiff=enabled -Dgif=enabled \
  -Dtests=false -Dinstalled_tests=false -Dintrospection=disabled -Dman=false -Dthumbnailer=disabled
meson compile -C "$build_dir/pixbuf" -j4
meson install -C "$build_dir/pixbuf"
# Preserve the custom GdkPixbuf build while collecting dynamically linked dependencies.
cp -an /usr/lib/* "$prefix/lib/"
cp -an /lib/* "$prefix/lib/"
# Keep linker tooling and package-manager state out of the application runtime.
rm -rf "$prefix/lib/bfd-plugins" "$prefix/lib/apk"
for resource in glib-2.0 gtk-4.0 mime misc ghostscript fonts fontconfig; do
  if [ -d "/usr/share/$resource" ]; then cp -a "/usr/share/$resource" "$prefix/share/"; fi
done
python3 "$root/tools/runtime-manifest.py" "$prefix" "$build_dir/core"
export GSETTINGS_SCHEMA_DIR="$prefix/share/glib-2.0/schemas"
export MAGIC="$prefix/share/misc/magic.mgc"
export GDK_PIXBUF_MODULEDIR="$prefix/lib/gdk-pixbuf-2.0/2.10.0/loaders"
"$prefix/bin/gdk-pixbuf-query-loaders" > "$build_dir/loaders.cache"
export GDK_PIXBUF_MODULE_FILE="$build_dir/loaders.cache"
meson test -C "$build_dir/core" --print-errorlogs
printf '%s\n' "$prefix"
