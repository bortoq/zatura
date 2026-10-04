#!/usr/bin/env python3
"""Archive application files only; dependencies must be installed by the host."""
import argparse
from pathlib import Path
import re
import tarfile
import tempfile
from importlib.machinery import SourceFileLoader
core = SourceFileLoader('native_packager', str(Path(__file__).with_name('package-linux.py'))).load_module()
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--stage', type=Path, required=True)
p.add_argument('--output', type=Path, default=Path('dist'))
a = p.parse_args(); a.output.mkdir(parents=True, exist_ok=True)
version = re.search(r"version: '([^']+)'", (core.ROOT/'meson.build').read_text())[1]
with tempfile.TemporaryDirectory() as work:
    directory = Path(work)
    core.install_core(a.stage.resolve(), directory)
    with tarfile.open(a.output/f'zatura-{version}-linux-x86_64.tar.gz', 'w:gz') as archive:
        archive.add(directory/'usr', arcname='usr')
