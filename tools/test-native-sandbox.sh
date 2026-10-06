#!/bin/sh
# SPDX-License-Identifier: Zlib
set -eu
cd "$(dirname "$0")/.."
meson setup --reconfigure build/native-sandbox ${MESON_NATIVE_ARGS:-} \
  --force-fallback-for=girara --default-library=static -Dgirara:docs=disabled \
  -Dseccomp=enabled -Dlandlock=enabled -Dmanpages=disabled -Dsynctex=disabled \
  -Dconvert-icon=disabled -Dshell-completions=disabled \
  -Dtests-x11=enabled -Dtests-wayland=disabled
meson compile -C build/native-sandbox -j "${JOBS:-4}"
export ZATURA_PLUGINS_PATH="$(pwd)/build/native-stage/usr/lib/zathura"
meson test -C build/native-sandbox --print-errorlogs seccomp-open xvfb_landlock_startup
python3 tools/require-engine-tests.py build/native-sandbox/meson-logs/testlog.json \
  seccomp-open xvfb_landlock_startup
