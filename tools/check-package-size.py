#!/usr/bin/env python3
"""Fail a packaging build when its generated artifact exceeds the byte budget."""
import argparse
from pathlib import Path
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('artifact', type=Path)
p.add_argument('budget', type=int)
a = p.parse_args()
size = a.artifact.stat().st_size
if size > a.budget:
    raise SystemExit(f'{a.artifact}: {size} bytes exceeds {a.budget}')
print(f'{a.artifact}: {size} bytes (budget {a.budget})')
