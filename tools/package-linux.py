#!/usr/bin/env python3
"""Package an already generated AppDir as deb, Arch binary package or Flatpak."""
import argparse
import configparser
import hashlib
import os
import pathlib
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time

root = pathlib.Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('format', choices=['deb','arch','flatpak'])
p.add_argument('--appdir', type=pathlib.Path, default=root/'dist/Zatura.AppDir')
p.add_argument('--output', type=pathlib.Path, default=root/'dist')
a = p.parse_args()
appdir = a.appdir.resolve(); output = a.output.resolve(); output.mkdir(parents=True,exist_ok=True)
version = (appdir/'VERSION').read_text().strip()
if not (appdir/'runtime/bin/zatura').is_file(): p.error('AppDir has no executable')
appid = 'io.github.bortoq.zatura'

def install_tree(directory):
    directory.chmod(0o755)
    shutil.copytree(appdir, directory/'opt/zatura', symlinks=True)
    binpath = directory/'usr/bin'; binpath.mkdir(parents=True)
    launcher = binpath/'zatura'; launcher.write_text('#!/bin/sh\nexec /opt/zatura/AppRun "$@"\n'); launcher.chmod(0o755)
    applications = directory/'usr/share/applications'; applications.mkdir(parents=True)
    desktop = (appdir/'zatura.desktop').read_text().replace('Exec=AppRun %U','Exec=zatura %U').replace('Icon=zatura',f'Icon={appid}')
    (applications/(appid+'.desktop')).write_text(desktop)
    icons = directory/'usr/share/icons/hicolor/scalable/apps'; icons.mkdir(parents=True)
    shutil.copy2(appdir/'zatura.svg',icons/(appid+'.svg'))
    licenses = directory/'usr/share/licenses/zatura'; licenses.mkdir(parents=True)
    shutil.copy2(appdir/'LICENSE',licenses/'LICENSE')
    for path in directory.rglob('*'):
        if not path.is_symlink():
            path.chmod(0o755 if path.is_dir() or path.stat().st_mode & 0o111 else 0o644)

with tempfile.TemporaryDirectory(prefix='zatura-package-',dir=output) as work:
    stage = pathlib.Path(work)
    if a.format in ('deb','arch'):
        install_tree(stage)
        size = sum(path.stat().st_size for path in stage.rglob('*') if path.is_file() and not path.is_symlink())
    if a.format == 'deb':
        control = stage/'DEBIAN'; control.mkdir()
        (control/'control').write_text(f'''Package: zatura
Version: {version}-1
Architecture: amd64
Maintainer: Zatura contributors <noreply@github.com>
Installed-Size: {(size+1023)//1024}
Depends: fontconfig
Recommends: fonts-dejavu-core
Section: graphics
Priority: optional
Homepage: https://github.com/bortoq/zatura
Description: document viewer with physical shortcuts and book reflow
 Includes a private musl runtime and document engines.
''')
        archive = output/f'zatura_{version}-1_amd64.deb'
        subprocess.run(['dpkg-deb','--root-owner-group','-Zxz','-z3','--build',str(stage),str(archive)],check=True)
    elif a.format == 'arch':
        (stage/'.PKGINFO').write_text(f'''pkgname = zatura-bin
pkgbase = zatura-bin
pkgver = {version}-1
pkgdesc = Document viewer with physical shortcuts and book reflow
url = https://github.com/bortoq/zatura
builddate = {int(os.environ.get('SOURCE_DATE_EPOCH',time.time()))}
packager = Zatura contributors
size = {size}
arch = x86_64
license = Zlib
depend = bash
depend = coreutils
depend = fontconfig
optdepend = ttf-dejavu: document fonts
provides = zatura
conflict = zatura
''')
        archive = output/f'zatura-bin-{version}-1-x86_64.pkg.tar.zst'
        def owner(info): info.uid=info.gid=0;info.uname=info.gname='root';return info
        tarpath = stage.parent/(stage.name+'.tar')
        try:
            with tarfile.open(tarpath,'w') as tar:
                for path in stage.iterdir(): tar.add(path,arcname=path.name,filter=owner)
            subprocess.run(['zstd','-q','-T2','-f',str(tarpath),'-o',str(archive)],check=True)
        finally: tarpath.unlink(missing_ok=True)
        portable = output/f'zatura-{version}-linux-x86_64.tar.gz'
        checksum = hashlib.file_digest(portable.open('rb'),'sha256').hexdigest()
        recipe = (root/'packaging/aur/PKGBUILD.in').read_text().replace('@VERSION@',version).replace('@SHA256@',checksum)
        (output/'PKGBUILD').write_text(recipe)
        srcinfo = (root/'packaging/aur/SRCINFO.in').read_text().replace('@VERSION@',version).replace('@SHA256@',checksum)
        (output/'.SRCINFO').write_text(srcinfo)
        with tarfile.open(output/f'zatura-aur-{version}.tar.gz','w:gz') as tar:
            for name in ('PKGBUILD','.SRCINFO'): tar.add(output/name,arcname=name)
    else:
        subprocess.run(['flatpak','build-init','--arch=x86_64',str(stage),appid,
                        'org.freedesktop.Platform','org.freedesktop.Platform','25.08'],check=True)
        files = stage/'files'
        shutil.copytree(appdir,files/'lib/zatura',symlinks=True)
        (files/'bin').mkdir()
        launcher = files/'bin/zatura';launcher.write_text('#!/bin/sh\nexec /app/lib/zatura/AppRun "$@"\n');launcher.chmod(0o755)
        applications = files/'share/applications';applications.mkdir(parents=True)
        desktop = (appdir/'zatura.desktop').read_text().replace('Exec=AppRun %U','Exec=zatura %U').replace('Icon=zatura',f'Icon={appid}')
        (applications/(appid+'.desktop')).write_text(desktop)
        icons = files/'share/icons/hicolor/scalable/apps';icons.mkdir(parents=True)
        shutil.copy2(appdir/'zatura.svg',icons/(appid+'.svg'))
        subprocess.run(['flatpak','build-finish','--command=zatura','--share=ipc','--socket=wayland',
                        '--socket=fallback-x11','--device=dri','--filesystem=home',str(stage)],check=True)
        repo = output/'flatpak-repo'
        export = ['flatpak','build-export','--arch=x86_64',str(repo),str(stage),'stable']
        # Let Flatpak initialize a fresh repository before changing its reserve.
        # This only affects our generated export repository, never user installs.
        config_path = repo/'config'
        exported = False
        if not config_path.exists():
            result = subprocess.run(export, text=True, capture_output=True)
            sys.stdout.write(result.stdout); sys.stderr.write(result.stderr)
            if result.returncode and ('min-free-space-percent' not in result.stderr or not config_path.exists()):
                result.check_returncode()
            exported = result.returncode == 0
        # The OSTree percentage default can reserve several GiB on large disks.
        config = configparser.ConfigParser()
        config.read(config_path)
        if not config.has_section('core'): config.add_section('core')
        config['core'].update(repo_version='1', mode='archive-z2',
                              **{'min-free-space-percent': '0', 'min-free-space-size': '256MB'})
        with config_path.open('w') as stream: config.write(stream)
        if not exported:
            subprocess.run(export,check=True)
        archive = output/f'zatura-{version}-x86_64.flatpak'
        subprocess.run(['flatpak','build-bundle','--arch=x86_64','--runtime-repo=https://flathub.org/repo/flathub.flatpakrepo',
                        str(repo),str(archive),appid,'stable'],check=True)
print(archive)
