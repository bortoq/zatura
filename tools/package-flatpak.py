#!/usr/bin/env python3
"""Export a native viewer with its PDF engine against the shared GNOME Platform (no embedded runtime).

For source builds use the flatpak-builder manifest instead. This path checks a
native glibc binary inside GNOME Platform before exporting it.
"""
import argparse
from importlib.machinery import SourceFileLoader
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import configparser
core = SourceFileLoader('native_packager', str(Path(__file__).with_name('package-linux.py'))).load_module()
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--stage', type=Path, required=True)
p.add_argument('--library-dir', type=Path, help='Linked libraries from the matching build distro (e.g. Zatura.AppDir/usr/lib)')
p.add_argument('--output', type=Path, default=Path('dist'))
a = p.parse_args(); output = a.output.resolve(); output.mkdir(parents=True, exist_ok=True)
version = re.search(r"version: '([^']+)'", (core.ROOT/'meson.build').read_text())[1]
with tempfile.TemporaryDirectory(prefix='flatpak-stage-', dir=output) as work:
    stage = Path(work)
    subprocess.run(['flatpak', 'build-init', '--arch=x86_64', str(stage), core.APPID,
                    'org.gnome.Platform', 'org.gnome.Platform', '49'], check=True)
    payload = stage/'payload'; payload.mkdir()
    core.install_core(a.stage.resolve(), payload)
    shutil.copytree(payload/'usr', stage/'files', dirs_exist_ok=True)
    shutil.rmtree(payload)
    # Native builds use /usr; locate their engines under /app at runtime.
    executable = stage/'files/bin/zatura'
    executable.rename(executable.with_name('zatura-bin'))
    executable.write_text('#!/bin/sh\nexport ZATURA_RUNTIME_DIR=/app\nexport LD_LIBRARY_PATH=/app/lib\nexec /app/bin/zatura-bin "$@"\n')
    executable.chmod(0o755)
    core_linked = subprocess.run(['flatpak', 'build', str(stage), 'ldd', '/app/bin/zatura-bin'],
                                 capture_output=True, text=True, check=True)
    if 'not found' in core_linked.stdout:
        raise SystemExit('Native core dependencies are missing from GNOME Platform')
    # GNOME Platform need not ship Poppler or its exact distro dependencies.
    # Copy only libraries missing from the platform, never replace platform GTK.
    engine = next((stage/'files').rglob('libpdf-poppler.so'))
    native_engine = next(a.stage.resolve().rglob('libpdf-poppler.so'))
    if a.library_dir:
        libraries = {p.name: str(p.resolve()) for p in a.library_dir.iterdir() if p.is_file()}
    else:
        native = subprocess.run(['ldd', str(native_engine)], capture_output=True, text=True, check=True)
        libraries = dict(re.findall(r'^\s*(\S+) => (/\S+)', native.stdout, re.M))
    notices = stage/'files/share/licenses/zatura/dependencies'
    notices.mkdir(parents=True, exist_ok=True)
    if a.library_dir:
        source_notices = a.library_dir.parent/'share/licenses/zatura/dependencies'
        if source_notices.is_dir(): shutil.copytree(source_notices, notices, dirs_exist_ok=True)
    for attempt in range(len(libraries) + 1):
        linked = subprocess.run(['flatpak', 'build', '--env=LD_LIBRARY_PATH=/app/lib', str(stage), 'ldd',
                                 '/app/'+str(engine.relative_to(stage/'files'))],
                                capture_output=True, text=True, check=True)
        missing = re.findall(r'^\s*(\S+) => not found', linked.stdout, re.M)
        if not missing:
            break
        for soname in missing:
            if soname not in libraries:
                raise SystemExit(f'Cannot resolve Flatpak dependency {soname}')
            source = Path(libraries[soname]).resolve()
            dest = stage/'files/lib'/soname
            if dest.exists():
                raise SystemExit(f'Flatpak cannot load already copied library {soname}')
            shutil.copy2(source, dest)
            owner = subprocess.run(['dpkg-query', '-S', str(source)], capture_output=True, text=True)
            if owner.returncode == 0:
                package = owner.stdout.split(': /')[0].splitlines()[0].split(':')[0]
                copyright = Path('/usr/share/doc')/package/'copyright'
                if copyright.is_file(): shutil.copy2(copyright, notices/(package+'.copyright'))
    else:
        raise SystemExit('Flatpak PDF dependency collection did not converge')
    subprocess.run(['flatpak', 'build', str(stage), '/app/bin/zatura', '--version'], check=True)
    subprocess.run(['flatpak', 'build-finish', '--command=zatura', '--share=ipc', '--socket=wayland',
                    '--socket=fallback-x11', '--device=dri', str(stage)], check=True)
    smoke = ['python3', str(core.ROOT/'tools/check-viewer-pdf.py'), '--flatpak-build', str(stage)]
    if shutil.which('xvfb-run'):
        smoke = ['xvfb-run', '-a'] + smoke
    subprocess.run(smoke, check=True)
    repo = output/'flatpak-repo'
    export = ['flatpak', 'build-export', str(repo), str(stage), 'stable']
    result = subprocess.run(export, text=True, capture_output=True)
    print(result.stdout, end=''); print(result.stderr, end='')
    if result.returncode:
        config_path = repo/'config'
        if 'min-free-space-percent' not in result.stderr or not config_path.exists():
            result.check_returncode()
        config = configparser.ConfigParser(); config.read(config_path)
        config['core']['min-free-space-percent'] = '0'
        config['core']['min-free-space-size'] = '256MB'
        with config_path.open('w') as stream: config.write(stream)
        subprocess.run(export, check=True)
    archive = output/f'zatura-{version}-x86_64.flatpak'
    subprocess.run(['flatpak', 'build-bundle', '--runtime-repo=https://flathub.org/repo/flathub.flatpakrepo',
                    str(repo), str(archive), core.APPID, 'stable'], check=True)
    if archive.stat().st_size > 15_000_000: raise SystemExit('Flatpak exceeds 15 MB')
    print(f'{archive}: {archive.stat().st_size} bytes')
