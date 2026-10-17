#!/usr/bin/env python3
"""Четыре варианта сервиса собираются в изолированной копии, не меняя checkout CI."""
from pathlib import Path
import argparse
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', action='store_true')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='broker-service-smoke-') as tmp:
    work = Path(tmp) / 'repo'
    shutil.copytree(root, work, ignore=shutil.ignore_patterns('.git', '.venv*', 'build-*', '__pycache__', '.cache', '.ccache'))
    for pg, grpc in [(False, False), (True, False), (False, True), (True, True)]:
        name = f'smoke_{int(pg)}{int(grpc)}'
        flags = (['--postgresql'] if pg else []) + (['--grpc'] if grpc else [])
        subprocess.run([sys.executable, 'scripts/create_service.py', name, *flags], cwd=work, check=True)
        if args.build:
            subprocess.run(['make', 'test-debug', f'SERVICE={name}'], cwd=work, check=True)
