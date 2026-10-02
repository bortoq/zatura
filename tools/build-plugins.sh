#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
prefix=${1:-"$root/build/runtime"}
export PKG_CONFIG_PATH="$prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
for plugin in pdf-mupdf djvu ps cb; do
  if [ "$plugin" = pdf-mupdf ]; then option=-Dpdf=disabled; else option=; fi
  meson setup --reconfigure "$root/build/plugins/$plugin" "$root/plugins/$plugin" --prefix "$prefix" --libdir lib -Dbuildtype=release $option
  meson compile -C "$root/build/plugins/$plugin" -j4
  meson install -C "$root/build/plugins/$plugin"
done
