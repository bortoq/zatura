#!/usr/bin/env python3
"""Open a real PDF with an installed viewer and require a completed render.

Run under Xvfb or an existing display. Supply the viewer command, or use
--flatpak-build PATH to test an unexported Flatpak installation.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import signal
import time

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--flatpak-build', type=Path)
p.add_argument('command', nargs=argparse.REMAINDER)
a = p.parse_args()
with tempfile.TemporaryDirectory(prefix='zatura-pdf-smoke-') as work:
    directory = Path(work)
    objects = [b'<< /Type /Catalog /Pages 2 0 R >>',
               b'<< /Type /Pages /Kids [3 0 R] /Count 1 >>',
               b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 64 64] /Resources << >> /Contents 4 0 R >>',
               b'<< /Length 23 >>\nstream\n1 0 0 rg 0 0 64 64 re f\nendstream']
    pdf = bytearray(b'%PDF-1.4\n')
    offsets = [0]
    for number, obj in enumerate(objects, 1):
        offsets.append(len(pdf))
        pdf.extend(f'{number} 0 obj\n'.encode() + obj + b'\nendobj\n')
    xref = len(pdf)
    pdf.extend(b'xref\n0 5\n0000000000 65535 f \n')
    for offset in offsets[1:]:
        pdf.extend(f'{offset:010d} 00000 n \n'.encode())
    pdf.extend(f'trailer\n<< /Size 5 /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n'.encode())
    fixture = directory / 'page.pdf'
    fixture.write_bytes(pdf)
    command = a.command
    if a.flatpak_build:
        command = ['flatpak', 'build', '--share=ipc', '--socket=x11',
                   f'--filesystem={directory}', str(a.flatpak_build), '/app/bin/zatura']
    if not command:
        p.error('a viewer command or --flatpak-build is required')
    for name in ('config', 'data', 'cache'):
        (directory / name).mkdir()
    command += ['--log-level=debug', '--config-dir', str(directory/'config'),
                '--data-dir', str(directory/'data'), '--cache-dir', str(directory/'cache'), str(fixture)]
    env = dict(os.environ, GTK_A11Y='none', GSK_RENDERER='cairo', GSETTINGS_BACKEND='memory')
    log = directory / 'viewer.log'
    with log.open('w') as stream:
        # Keep the sandbox's stdio on a pipe. The parent owns the log file;
        # the parser never inherits a writable filesystem descriptor.
        viewer = subprocess.Popen(command, env=env, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, start_new_session=True)
        os.set_blocking(viewer.stdout.fileno(), False)
        def collect_output():
            while True:
                try:
                    chunk = os.read(viewer.stdout.fileno(), 65536)
                except BlockingIOError:
                    break
                if not chunk:
                    break
                stream.write(chunk.decode('utf-8', errors='replace'))
            stream.flush()
        rendered = False
        try:
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                collect_output()
                output = log.read_text(errors='replace')
                if 'Emitting signal for page 1' in output or 'Rendered page 1 synchronously.' in output:
                    rendered = True
                    break
                if viewer.poll() is not None:
                    break
                time.sleep(0.1)
        finally:
            # AppImage/Flatpak wrappers can exit before their viewer child.
            # Stop the entire test process group; never block on an inherited pipe.
            for sig in (signal.SIGTERM, signal.SIGKILL):
                try:
                    os.killpg(viewer.pid, sig)
                except ProcessLookupError:
                    pass
                if sig == signal.SIGTERM:
                    try:
                        viewer.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        pass
            viewer.wait(timeout=3)
            collect_output()
            viewer.stdout.close()
    if not rendered:
        raise SystemExit('Installed viewer failed PDF rendering:\n' + log.read_text(errors='replace'))
    print('Installed viewer PDF render: OK')
