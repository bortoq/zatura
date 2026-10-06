#!/usr/bin/env python3
"""Fail CI if any required integration suite is missing, skipped or failing."""
import json
from pathlib import Path
import sys
results = [json.loads(line) for line in Path(sys.argv[1]).read_text().splitlines() if line.strip()]
for name in sys.argv[2:]:
    found = [row for row in results if row['name'].split(' / ')[-1].rsplit(':', 1)[-1] == name]
    if len(found) != 1 or found[0]['result'] != 'OK':
        raise SystemExit(f'Required integration test {name}: {found or "missing"}')
