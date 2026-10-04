#!/usr/bin/env python3
"""Remove generated build/package trees, guarding repository source files."""
import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--builddir', default='build')
p.add_argument('--distdir', default='dist')
a = p.parse_args()
tracked = subprocess.run(['git', 'ls-files', '-z'], cwd=ROOT, capture_output=True, check=True).stdout
tracked_paths = [ROOT / s.decode() for s in tracked.split(b'\0') if s]
targets = []
for value, kind in [(a.builddir, 'build'), (a.distdir, 'dist')]:
    original = Path(value)
    if not original.is_absolute(): original = ROOT / original
    if original.is_symlink():
        raise SystemExit(f'Refusing symlink cleanup target: {original}')
    target = original.resolve()
    if target == ROOT or not target.is_relative_to(ROOT) or target.parts[len(ROOT.parts)].startswith('.'):
        raise SystemExit(f'Refusing cleanup outside generated repository directories: {target}')
    if any(path.is_relative_to(target) for path in tracked_paths):
        raise SystemExit(f'Refusing to remove directory containing tracked source: {target}')
    if target.exists() and target.name not in ('build', 'build-ci', 'dist'):
        if kind == 'build':
            generated = (target/'meson-private/coredata.dat').exists()
        else:
            generated = any(target.glob('zatura_*.deb')) or (target/'Zatura.AppDir').exists()
        if not generated:
            raise SystemExit(f'Unrecognized generated directory: {target}')
    targets.append(target)
for target in targets:
    if target.exists():
        shutil.rmtree(target)
        print(f'Removed {target.relative_to(ROOT)}')
for cache in ROOT.rglob('__pycache__'):
    if cache.is_dir() and not cache.is_symlink() and not any(part.startswith('.') for part in cache.relative_to(ROOT).parts):
        if not any(path.is_relative_to(cache) for path in tracked_paths):
            shutil.rmtree(cache)
