#!/usr/bin/env python3
"""Pack an already installed, complete x86_64 musl runtime; no network or system changes."""
import argparse
import hashlib
import pathlib
import re
import shutil
import tarfile

root = pathlib.Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--runtime', type=pathlib.Path, default=root/'build/runtime')
parser.add_argument('--output', type=pathlib.Path, default=root/'dist')
args = parser.parse_args()
runtime = args.runtime.resolve()
required = ['bin/zatura', 'bin/gdk-pixbuf-query-loaders', 'lib/ld-musl-x86_64.so.1',
            'share/glib-2.0/schemas/gschemas.compiled', 'share/misc/magic.mgc']
missing = [name for name in required if not (runtime/name).is_file()]
if missing:
    parser.error('incomplete musl runtime: '+', '.join(missing))
version = re.search(r"version: '([^']+)'", (root/'meson.build').read_text())[1]
output = args.output.resolve()
appdir = output/'Zatura.AppDir'
if appdir.exists():
    # This is a generated, fixed-name directory inside the explicitly selected output.
    shutil.rmtree(appdir)
appdir.mkdir(parents=True)
shutil.copytree(runtime, appdir/'runtime', symlinks=True,
                ignore=shutil.ignore_patterns('include', '*.a', '*.la', 'pkgconfig', 'cmake'))
for path in (appdir/'runtime').rglob('*'):
    if path.is_symlink() and path.readlink().is_absolute():
        raise SystemExit(f'nonportable absolute symlink: {path}')
shutil.copy2(root/'packaging/AppRun', appdir/'AppRun')
(appdir/'AppRun').chmod(0o755)
shutil.copy2(root/'data/io.github.bortoq.zatura.svg', appdir/'zatura.svg')
(appdir/'zatura.desktop').write_text('[Desktop Entry]\nType=Application\nName=Zatura\nExec=AppRun %U\nIcon=zatura\nTerminal=false\nCategories=Office;Viewer;\n')
(appdir/'VERSION').write_text(version+'\n')
shutil.copy2(root/'LICENSE', appdir/'LICENSE')
shutil.copytree(root/'doc', appdir/'docs', ignore=shutil.ignore_patterns('Doxyfile', 'meson.build'))
shutil.copy2(root/'CHANGELOG.md', appdir/'CHANGELOG.md')
# Original copyright notices and exact source revisions for modified plugins.
licenses = appdir/'licenses'; shutil.copytree(root/'packaging/licenses', licenses)
for name in ['pdf-mupdf', 'djvu', 'ps', 'cb']:
    shutil.copy2(root/'plugins'/name/'LICENSE', licenses/(name+'.txt'))
shutil.copy2(root/'plugins/README.md', licenses/'plugin-revisions.md')
(licenses/'RUNTIME.md').write_text('Runtime built from Alpine Linux 3.24 packages. See BUILD-MANIFEST.json for installed packages and LICENSE for Zatura. Modified plugin sources accompany this release. See docs/SOURCES.md for dependency archives and build recipes.\n')
if (runtime/'BUILD-MANIFEST.json').exists():
    shutil.copy2(runtime/'BUILD-MANIFEST.json', appdir/'BUILD-MANIFEST.json')
archive = output/f'zatura-{version}-linux-x86_64.tar.gz'
with tarfile.open(archive, 'w:gz', compresslevel=6) as tar:
    tar.add(appdir, arcname=f'zatura-{version}')
print(archive)
print('sha256:', hashlib.sha256(archive.read_bytes()).hexdigest())
