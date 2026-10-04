#!/bin/sh
# SPDX-License-Identifier: Zlib
# Run in the target distribution. System GTK/GLib are used, Girara core is static.
set -eu
cd "$(dirname "$0")/.."
build_dir=${NATIVE_BUILD:-build/native}
stage=${NATIVE_STAGE:-build/native-stage}
meson setup "$build_dir" ${MESON_NATIVE_ARGS:-} --prefix=/usr --libdir=lib --buildtype=release \
  --default-library=static --force-fallback-for=girara \
  -Dmanpages=enabled -Dsynctex=disabled -Dseccomp=disabled \
  -Dlandlock=disabled -Dconvert-icon=disabled -Dshell-completions=disabled \
  -Dtests-x11=enabled -Dtests-wayland=disabled -Dgirara:docs=disabled
meson compile -C "$build_dir" -j "${JOBS:-4}"
DESTDIR="$(pwd)/$stage" meson install -C "$build_dir" --strip
meson test -C "$build_dir" --print-errorlogs
