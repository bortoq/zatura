#!/usr/bin/env python3
"""Package a native staged install; never bundle a private runtime."""
import argparse
import gzip
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
APPID = 'io.github.bortoq.zatura'

def run(*args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)

def install_core(source, target):
    """Whitelist application files and the mandatory compatible PDF engine."""
    binary = source / 'usr/bin/zatura'
    if not binary.is_file():
        raise SystemExit('Missing native staged usr/bin/zatura; use DESTDIR with prefix=/usr')
    elf = binary.read_bytes()
    if not elf.startswith(b'\x7fELF') or b'ld-musl-' in elf:
        raise SystemExit('A native glibc executable is required, not the private musl build')
    paths = ['usr/bin/zatura', f'usr/share/applications/{APPID}.desktop',
             f'usr/share/icons/hicolor/scalable/apps/{APPID}.svg',
             f'usr/share/metainfo/{APPID}.metainfo.xml',
             f'usr/share/dbus-1/interfaces/{APPID}.xml']
    for name in paths:
        src = source / name
        if not src.is_file():
            raise SystemExit(f'Missing staged application file: {name}')
        dest = target / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)
    for section, name in [('man1', 'zatura.1'), ('man5', 'zaturarc.5')]:
        src = source / f'usr/share/man/{section}/{name}'
        if not src.is_file():
            raise SystemExit(f'Missing staged manual: {src}')
        dest = target / f'usr/share/man/{section}/{name}.gz'
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(gzip.compress(src.read_bytes(), mtime=0))
    dest = target / 'usr/share/licenses/zatura/LICENSE'
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / 'LICENSE', dest)
    shutil.copy2(ROOT / 'subprojects/girara/LICENSE', dest.with_name('girara'))
    engines = sorted(source.glob('usr/lib*/**/zathura/libpdf-poppler.so')) + sorted(source.glob('usr/lib*/**/zatura/libpdf-poppler.so'))
    if len(engines) != 1:
        raise SystemExit('Stage must contain exactly one compatible libpdf-poppler.so; run build-native.sh')
    engine = engines[0]
    dest = target / engine.relative_to(source)
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(engine, dest)
    license_source = ROOT / 'build/sources/pdf-poppler/LICENSE'
    if not license_source.is_file():
        raise SystemExit('Missing pinned PDF Poppler license source')
    shutil.copy2(license_source, target / 'usr/share/licenses/zatura/pdf-poppler')
    for path in target.rglob('*'):
        path.chmod(0o755 if path.is_dir() or path == target / 'usr/bin/zatura' else 0o644)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('format', choices=['deb', 'arch'])
    parser.add_argument('--stage', type=Path, default=ROOT / 'build/native-stage')
    parser.add_argument('--output', type=Path, default=ROOT / 'dist')
    args = parser.parse_args()
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    epoch = int(os.environ.get('SOURCE_DATE_EPOCH') or subprocess.check_output(
        ['git', '-C', str(ROOT), 'show', '-s', '--format=%ct', 'HEAD'], text=True).strip())
    os.environ['SOURCE_DATE_EPOCH'] = str(epoch)
    version = re.search(r"version: '([^']+)'", (ROOT / 'meson.build').read_text())[1]
    with tempfile.TemporaryDirectory(prefix='native-package-', dir=output) as directory:
        stage = Path(directory)
        install_core(args.stage.resolve(), stage)
        size = sum(p.stat().st_size for p in stage.rglob('*') if p.is_file())
        for path in stage.rglob('*'):
            os.utime(path, (epoch, epoch))
        if args.format == 'deb':
            # Resolve actual DT_NEEDED and symbol versions against the build distro.
            with tempfile.TemporaryDirectory(prefix='shlibdeps-') as depsdir:
                debian = Path(depsdir) / 'debian'; debian.mkdir()
                (debian / 'control').write_text('Source: zatura\n\nPackage: zatura\nArchitecture: any\n')
                result = run('dpkg-shlibdeps', '-O', str(stage / 'usr/bin/zatura'),
                             *[str(p) for p in stage.rglob('libpdf-poppler.so')],
                             cwd=depsdir, capture_output=True, text=True)
            deps = result.stdout.strip().removeprefix('shlibs:Depends=')
            parts = deps.split(', ')
            for name, minimum in [('libgtk-4-1', '4.12'), ('libglib2.0-0t64', '2.84'), ('libsqlite3-0', '3.35')]:
                for index, part in enumerate(parts):
                    if part.split(' ')[0] != name: continue
                    match = re.search(r'\(>= ([^)]+)\)', part)
                    if match is None or subprocess.run(['dpkg', '--compare-versions', match[1], 'lt', minimum]).returncode == 0:
                        parts[index] = f'{name} (>= {minimum})'
                    break
                else: parts.append(f'{name} (>= {minimum})')
            deps = ', '.join(parts)
            control = stage / 'DEBIAN'; control.mkdir()
            (control / 'control').write_text(f'''Package: zatura
Version: {version}-1
Architecture: amd64
Maintainer: Zatura contributors <noreply@github.com>
Installed-Size: {(size + 1023) // 1024}
Depends: {deps}
Section: graphics
Priority: optional
Homepage: https://github.com/bortoq/zatura
Description: GTK4 document viewer with physical shortcuts
 Includes the compatible Poppler PDF engine; plugin API 8, ABI 9.
''')
            archive = output / f'zatura_{version}-1_amd64.deb'
            run('dpkg-deb', '--root-owner-group', '-Zxz', '-z9', '--build', str(stage), str(archive))
            limit = 1_000_000
        else:
            dependencies = ['glibc', 'gtk4>=4.12', 'glib2>=2.84', 'json-glib', 'file',
                            'sqlite>=3.35', 'xxhash', 'libxkbcommon', 'libarchive', 'cairo', 'poppler-glib']
            (stage / '.PKGINFO').write_text(f'''pkgname = zatura-bin
pkgbase = zatura-bin
pkgver = {version}-1
pkgdesc = GTK4 document viewer with physical shortcuts
url = https://github.com/bortoq/zatura
builddate = {epoch}
packager = Zatura contributors
size = {size}
arch = x86_64
license = Zlib
provides = zatura
conflict = zatura
''' + ''.join(f'depend = {dep}\n' for dep in dependencies))
            archive = output / f'zatura-bin-{version}-1-x86_64.pkg.tar.zst'
            tarpath = stage / 'payload.tar'
            def owner(info):
                info.uid = info.gid = 0; info.uname = info.gname = 'root'; info.mtime = epoch; return info
            with tarfile.open(tarpath, 'w') as tar:
                for name in ['usr', '.PKGINFO']:
                    tar.add(stage / name, arcname=name, filter=owner)
            run('zstd', '-q', '-19', '-T2', '-f', str(tarpath), '-o', str(archive))
            digest = hashlib.sha256(archive.read_bytes()).hexdigest()
            for name, template in [('PKGBUILD', 'PKGBUILD.in'), ('.SRCINFO', 'SRCINFO.in')]:
                text = (ROOT / 'packaging/aur' / template).read_text()
                (output / name).write_text(text.replace('@VERSION@', version).replace('@SHA256@', digest))
            with (output / f'zatura-aur-{version}.tar.gz').open('wb') as raw:
                with gzip.GzipFile(fileobj=raw, mode='wb', filename='', mtime=epoch) as compressed:
                    with tarfile.open(fileobj=compressed, mode='w') as tar:
                        for name in ['PKGBUILD', '.SRCINFO']:
                            tar.add(output / name, arcname=name, filter=owner)
            limit = 1_000_000
        if archive.stat().st_size > limit:
            raise SystemExit(f'Package exceeds size budget: {archive.stat().st_size} > {limit}')
        # Test the contents extracted from the final archive, including the engine.
        with tempfile.TemporaryDirectory(prefix='package-pdf-') as extracted:
            if args.format == 'deb':
                run('dpkg-deb', '-x', str(archive), extracted)
            else:
                run('tar', '--zstd', '-xf', str(archive), '-C', extracted)
            environment = dict(os.environ, ZATURA_RUNTIME_DIR=str(Path(extracted)/'usr'),
                               ZATURA_PLUGINS_PATH=':'.join(str(p.parent) for p in Path(extracted).rglob('libpdf-poppler.so')))
            run('xvfb-run', '-a', 'python3', str(ROOT/'tools/check-viewer-pdf.py'),
                str(Path(extracted)/'usr/bin/zatura'), env=environment)
        print(f'{archive}: {archive.stat().st_size} bytes')

if __name__ == '__main__':
    main()
