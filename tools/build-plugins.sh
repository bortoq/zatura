#!/bin/sh
# SPDX-License-Identifier: Zlib
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
prefix=${1:-"$root/build/runtime"}
build_root=${PLUGIN_BUILD_DIR:-"$root/build/plugins/native"}
mkdir -p "$build_root/pkgconfig"
# Discover the installed SDK, including Debian multiarch and Arch lib64.
# Relocate only our SDK .pc files for DESTDIR builds; system dependencies keep
# their original prefix. The prefix argument must name the installed SDK root.
python3 - "$prefix" "$build_root/pkgconfig" <<'PYCODE'
import pathlib, sys
prefix, target = map(pathlib.Path, sys.argv[1:])
for name in ('zathura', 'zatura', 'girara'):
    matches = sorted(prefix.glob(f'lib*/**/pkgconfig/{name}.pc'))
    if len(matches) != 1:
        raise SystemExit(f'Expected one {name}.pc below {prefix}, found {len(matches)}')
    lines = matches[0].read_text().splitlines()
    (target / f'{name}.pc').write_text('\n'.join(
        f'prefix={prefix}' if line.startswith('prefix=') else line for line in lines) + '\n')
PYCODE
export PKG_CONFIG_PATH="$build_root/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
[ "$(pkg-config --variable=apiversion zathura)" = 8 ]
[ "$(pkg-config --variable=abiversion zathura)" = 9 ]
# Poppler is the default PDF engine. Optional MuPDF supplies EPUB/FB2 reflow.
for plugin in ${PLUGINS:-pdf-poppler}; do
  option=
  source="$root/plugins/$plugin"
  if [ "$plugin" = pdf-poppler ]; then
    source="$root/build/sources/pdf-poppler"
    commit=165f37248f235bb192d154b06094c52ca92f5b2d
    if [ ! -d "$source" ]; then
      mkdir -p "$(dirname "$source")"
      git clone --quiet https://github.com/pwmt/zathura-pdf-poppler.git "$source"
      git -C "$source" checkout --quiet "$commit"
    fi
    [ "$(git -C "$source" rev-parse HEAD)" = "$commit" ] || {
      echo 'PDF Poppler source does not match the pinned revision' >&2; exit 1;
    }
  elif [ "$plugin" = pdf-mupdf ]; then
    case " ${PLUGINS:-pdf-poppler} " in
      *' pdf-poppler '*) option=-Dpdf=disabled ;;
      *) option=-Dpdf=enabled ;;
    esac
  elif [ "$plugin" != djvu ] && [ "$plugin" != ps ] && [ "$plugin" != cb ]; then
    echo "Unknown plugin: $plugin" >&2; exit 1
  fi
  meson setup --reconfigure "$build_root/$plugin" "$source" ${MESON_PLUGIN_ARGS:-} --prefix "$prefix" --libdir lib -Dbuildtype=release $option
  meson compile -C "$build_root/$plugin" -j "${JOBS:-4}"
  meson install -C "$build_root/$plugin" --strip
done
