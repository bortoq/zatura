#!/bin/sh
# SPDX-License-Identifier: Zlib
set -eu
cd "$(dirname "$0")/.."
build=build/engines
prefix="$(pwd)/build/engine-runtime"
meson setup --reconfigure "$build" ${MESON_ENGINE_ARGS:-} --prefix "$prefix" --libdir lib --buildtype=debugoptimized \
  --default-library=static --force-fallback-for=girara \
  -Dgirara:docs=disabled -Dmanpages=disabled -Dsynctex=disabled \
  -Dseccomp=enabled -Dlandlock=disabled \
  -Dconvert-icon=disabled -Dshell-completions=disabled \
  -Dtests-x11=enabled -Dtests-wayland=enabled
meson compile -C "$build" -j "${JOBS:-4}"
meson install -C "$build"
PLUGINS='pdf-poppler pdf-mupdf djvu ps cb' \
  PLUGIN_BUILD_DIR="$(pwd)/build/plugins/engines" ./tools/build-plugins.sh "$prefix"
export ZATURA_RUNTIME_DIR="$prefix"
meson test -C "$build" --print-errorlogs
python3 tools/require-engine-tests.py "$build/meson-logs/testlog.json" \
  xvfb_reflow xvfb_content_anchors xvfb_view_history xvfb_pdf_effects \
  xvfb_cache_budget seccomp-open sandbox-fds xvfb_sandbox weston_sandbox
