#!/usr/bin/env python3
"""Export a native core against the shared GNOME Platform (no embedded runtime).

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
    subprocess.run(['flatpak', 'build', str(stage), 'ldd', '/app/bin/zatura'], check=True)
    subprocess.run(['flatpak', 'build', str(stage), '/app/bin/zatura', '--version'], check=True)
    subprocess.run(['flatpak', 'build-finish', '--command=zatura', '--share=ipc', '--socket=wayland',
                    '--socket=fallback-x11', '--device=dri', '--filesystem=home', str(stage)], check=True)
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
