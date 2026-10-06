#!/usr/bin/env python3
"""Verify mandatory Landlock startup on both old and supported kernels."""
from pathlib import Path
import subprocess
import sys
import tempfile

probe, viewer, smoke = sys.argv[1:]
abi = int(subprocess.check_output([probe], text=True).strip())
if abi >= 8:
    subprocess.run([sys.executable, smoke, viewer], check=True)
else:
    with tempfile.TemporaryDirectory(prefix='zatura-landlock-') as work:
        result = subprocess.run([viewer, '--config-dir', work, '--data-dir', work,
                                 '--cache-dir', work], capture_output=True, text=True, timeout=15)
        if result.returncode == 0 or 'Failed to apply landlock write restriction.' not in result.stderr:
            raise SystemExit(f'Landlock ABI {abi}: strict sandbox did not refuse startup\n{result.stderr}')
    print(f'Landlock ABI {abi}: strict sandbox refused startup as required')
