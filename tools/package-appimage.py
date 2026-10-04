#!/usr/bin/env python3
"""Build a glibc AppImage with linuxdeploy and only needed shared libraries."""
import argparse
from importlib.machinery import SourceFileLoader
from pathlib import Path
import re
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
core = SourceFileLoader('native_packager', str(root/'tools/package-linux.py')).load_module()
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--stage', type=Path, required=True)
p.add_argument('--output', type=Path, default=root/'dist')
p.add_argument('--linuxdeploy', default='linuxdeploy')
a = p.parse_args(); output = a.output.resolve(); output.mkdir(parents=True, exist_ok=True)
appdir = output/'Zatura.AppDir'
if appdir.exists(): shutil.rmtree(appdir)
appdir.mkdir()
core.install_core(a.stage.resolve(), appdir)
# GTK4 resources are installed explicitly; linuxdeploy-plugin-gtk targets GTK2/3.
for schema in Path('/usr/share/glib-2.0/schemas').glob('org.gtk.*.xml'):
    dest = appdir/'usr/share/glib-2.0/schemas'/schema.name
    dest.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(schema, dest)
if (appdir/'usr/share/glib-2.0/schemas').exists():
    subprocess.run(['glib-compile-schemas', str(appdir/'usr/share/glib-2.0/schemas')], check=True)
# libmagic is a direct dependency. Its identification database is required too.
magic = Path('/usr/share/misc/magic.mgc')
if magic.exists():
    dest = appdir/'usr/share/misc/magic.mgc'; dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(magic.resolve(), dest)
version = re.search(r"version: '([^']+)'", (root/'meson.build').read_text())[1]
launcher = appdir/'AppRun'
shutil.copy2(root/'packaging/AppRun', launcher); launcher.chmod(0o755)
command = [a.linuxdeploy, '--appdir', str(appdir), '--executable', str(appdir/'usr/bin/zatura'),
           '--desktop-file', str(appdir/f'usr/share/applications/{core.APPID}.desktop'),
           '--icon-file', str(appdir/f'usr/share/icons/hicolor/scalable/apps/{core.APPID}.svg'),
           '--custom-apprun', str(root/'packaging/AppRun')]
# Drivers and optional media/TLS backends belong to the host. Never ship a DRI stack.
excluded = ['libLLVM*', 'libgallium*', 'libMesa*', '*_dri.so', 'libgs.so*',
            'libgstreamer*', 'libgst*', 'libgnutls*', 'libnss3*', 'libnssutil3*',
            'libsmime3*', 'libssl3.so*', 'libdav1d*', 'libSPIRV*']
for pattern in excluded: command += ['--exclude-library', pattern]

import os
env = os.environ.copy(); env.update(ARCH='x86_64', APPIMAGE_EXTRACT_AND_RUN='1',
                                  OUTPUT=f'Zatura-{version}-x86_64.AppImage')
subprocess.run(command, cwd=output, env=env, check=True)
# linuxdeploy's historic blacklist also excludes text/font and C++ libraries.
# GTK4/Pango need newer HarfBuzz symbols; keep the complete linked closure.
# These are actual DT_NEEDED libraries, not optional driver/media stacks.
import fnmatch
linked = subprocess.run(['ldd', str(a.stage.resolve()/'usr/bin/zatura')], capture_output=True, text=True, check=True)
for soname, filename in re.findall(r'^\s*(\S+) => (/\S+)', linked.stdout, re.M):
    if any(fnmatch.fnmatch(soname, pattern) for pattern in excluded):
        raise SystemExit(f'Excluded library is genuinely linked: {soname}; review before bundling')
    dest = appdir/'usr/lib'/soname
    if not dest.exists():
        shutil.copyfile(Path(filename).resolve(), dest)
        dest.chmod(0o755)
# The GTK/GLib baseline requires a recent build distro. Include only glibc's
# linked libraries and loader so the AppImage also starts on older hosts.
for soname in ['libc.so.6', 'libm.so.6', 'ld-linux-x86-64.so.2']:
    library = Path('/lib/x86_64-linux-gnu')/soname
    if not library.exists(): library = Path('/usr/lib')/soname
    if not library.exists(): raise SystemExit(f'Missing native glibc library: {soname}')
    dest = appdir/'usr/lib'/soname
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(library.resolve(), dest)
    dest.chmod(0o755)

licenses = appdir/'usr/share/licenses/zatura/dependencies'; licenses.mkdir(parents=True, exist_ok=True)
for library in (appdir/'usr/lib').glob('*'):
    if not library.is_file(): continue
    original = Path('/usr/lib/x86_64-linux-gnu')/library.name
    if not original.exists(): continue
    owner = subprocess.run(['dpkg-query', '-S', str(original.resolve())], capture_output=True, text=True)
    if owner.returncode:
        owner = subprocess.run(['dpkg-query', '-S', str(original.resolve()).replace('/usr/lib/', '/lib/')], capture_output=True, text=True)
    if owner.returncode: continue
    package = owner.stdout.split(': /')[0].splitlines()[0].split(':')[0]
    copyright = Path('/usr/share/doc')/package/'copyright'
    if copyright.exists(): shutil.copyfile(copyright, licenses/(package+'.copyright'))
manifest = subprocess.run(['dpkg-query', '-W', '-f=${binary:Package}\t${Version}\t${source:Package}\t${source:Version}\n'],
                          capture_output=True, text=True, check=True)
(appdir/'usr/share/licenses/zatura/build-packages.tsv').write_text(manifest.stdout)
shutil.copy2(root/'doc/SOURCES.md', appdir/'usr/share/licenses/zatura/SOURCES.md')
# A missing library is an error, even if the size target is met.
result = subprocess.run([str(appdir/'usr/lib/ld-linux-x86-64.so.2'), '--library-path', str(appdir/'usr/lib'), '--list', str(appdir/'usr/bin/zatura')], env=dict(env, LD_LIBRARY_PATH=str(appdir/'usr/lib')),
                        capture_output=True, text=True, check=True)
(output/'appimage-linked-libraries.txt').write_text(result.stdout)
if 'not found' in result.stdout: raise SystemExit('AppImage contains unresolved ELF dependencies')
# linuxdeploy's patchelf pass must never touch glibc's loader. Its official
# output plugin packages the already deployed AppDir without modifying ELF.
import tempfile
plugin = shutil.which('linuxdeploy-plugin-appimage')
with tempfile.TemporaryDirectory(prefix='linuxdeploy-output-') as extraction:
    if plugin is None:
        deploy = shutil.which(a.linuxdeploy) or a.linuxdeploy
        deploy = str(Path(deploy).resolve())
        with (output/'linuxdeploy-output-extract.log').open('w') as log:
            subprocess.run([deploy, '--appimage-extract'], cwd=extraction, stdout=log, stderr=log, check=True)
        plugin = str(Path(extraction)/'squashfs-root/plugins/linuxdeploy-plugin-appimage/usr/bin/linuxdeploy-plugin-appimage')
    subprocess.run([plugin, '--appdir', str(appdir)], cwd=output, env=env, check=True)
archive = output/env['OUTPUT']
if archive.stat().st_size > 40_000_000: raise SystemExit(f'AppImage exceeds 40 MB: {archive.stat().st_size}')
print(f'{archive}: {archive.stat().st_size} bytes')
