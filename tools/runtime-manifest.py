#!/usr/bin/env python3
"""Resolve absolute runtime links and record the exact build environment."""
import json
import os
import pathlib
import subprocess
import sys
prefix = pathlib.Path(sys.argv[1]).resolve()
for path in prefix.rglob('*'):
    if path.is_symlink() and path.readlink().is_absolute():
        target = str(path.readlink())
        if target.startswith('/usr/lib/'):
            bundled = prefix/'lib'/target.removeprefix('/usr/lib/')
        elif target.startswith('/lib/'):
            bundled = prefix/'lib'/target.removeprefix('/lib/')
        else:
            raise SystemExit(f'Unhandled absolute runtime symlink: {path} -> {target}')
        path.unlink()
        path.symlink_to(os.path.relpath(bundled,path.parent))
manifest = {
    'architecture':'x86_64', 'distribution':pathlib.Path('/etc/alpine-release').read_text().strip(),
    'compiler':subprocess.check_output(['cc','--version'],text=True).splitlines()[0],
    'packages':sorted(subprocess.check_output(['apk','info','-v'],text=True).splitlines()),
    'build_options':json.loads(subprocess.check_output(['meson','introspect','--buildoptions',sys.argv[2]],text=True)),
    'girara_revision':'e120cc0c4f486af935f09a07420606925a6f9305',
    'poppler_plugin_revision':'165f37248f235bb192d154b06094c52ca92f5b2d',
    'gdk_pixbuf_version':'2.44.7',
    'gdk_pixbuf_source_sha256':'172f80e3626ec31520a970400f1a3694e04718f6c2cd2885f75250fb5a6995a4',
    'dependency_sources':'https://gitlab.alpinelinux.org/alpine/aports/-/tree/3.24-stable',
}
(prefix/'BUILD-MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
