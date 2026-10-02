#!/usr/bin/env python3
"""Archive the committed project plus optional external build sources."""
import argparse
import io
import pathlib
import re
import subprocess
import tarfile

root = pathlib.Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=pathlib.Path, default=root/'dist')
parser.add_argument('--revision', default='HEAD')
parser.add_argument('--extra-source', action='append', default=[], metavar='NAME=PATH')
args = parser.parse_args()
version = re.search(r"version: '([^']+)'", subprocess.check_output(
    ['git', 'show', f'{args.revision}:meson.build'], cwd=root, text=True))[1]
prefix = f'zatura-{version}-source'
extras = []
for item in args.extra_source:
    name, separator, path = item.partition('=')
    if not separator or not re.fullmatch(r'[A-Za-z0-9._-]+', name) or name in ('.', '..'):
        parser.error('--extra-source expects a simple NAME=PATH')
    source = pathlib.Path(path).resolve()
    if not source.is_dir():
        parser.error(f'missing source directory: {source}')
    extras.append((name, source))
args.output.mkdir(parents=True, exist_ok=True)
archive = args.output/f'{prefix}.tar.gz'
committed = subprocess.check_output(['git', 'archive', args.revision], cwd=root)
with tarfile.open(archive, 'w:gz') as output:
    with tarfile.open(fileobj=io.BytesIO(committed), mode='r:') as project:
        for info in project:
            info.name = f'{prefix}/{info.name}'
            output.addfile(info, project.extractfile(info) if info.isfile() else None)
    def include(info):
        # Exclude repository metadata and generated build output in extra trees.
        if any(part in ('.git', '__pycache__', 'build') for part in pathlib.PurePosixPath(info.name).parts):
            return None
        return info
    for name, source in extras:
        output.add(source, arcname=f'{prefix}/external-sources/{name}', filter=include)
print(archive.resolve())
