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
    shutil.copytree(
        root,
        work,
        ignore=shutil.ignore_patterns(
            '.git',
            '.venv*',
            'build-*',
            '__pycache__',
            '.cache',
            '.ccache',
            'node_modules',
            'dist',
            'coverage',
        ),
    )
    for pg, grpc in [(False, False), (True, False), (False, True), (True, True)]:
        name = f'smoke_{int(pg)}{int(grpc)}'
        flags = (['--postgresql'] if pg else []) + (['--grpc'] if grpc else [])
        subprocess.run(
            [sys.executable, 'scripts/create_service.py', name, *flags], cwd=work, check=True
        )
        if args.build:
            subprocess.run(
                ['make', 'ci-build-debug', f'CI_BUILD_SERVICES={name}'], cwd=work, check=True
            )
            subprocess.run(
                [
                    'make',
                    'ci-test-debug',
                    f'CI_SERVICES={name}',
                    f'CI_RESULTS_DIR={tmp}/results/{name}',
                ],
                cwd=work,
                check=True,
            )

    # Подключаем зависимости к уже созданному сервису и повторяем настоящую сборку.
    library = work / 'libs/smoke_support'
    library.mkdir()
    (library / 'CMakeLists.txt').write_text('add_library(broker_smoke_support INTERFACE)\n')
    service = work / 'services/smoke_00'
    for pg, grpc in [(True, False), (True, True)]:
        (service / 'service.yaml').write_text(
            f'version: 1\nlibraries: [common, smoke_support]\npostgresql: {{enabled: {str(pg).lower()}, cluster: primary}}\ngrpc: {{enabled: {str(grpc).lower()}}}\n'
        )
        subprocess.run(
            [sys.executable, 'scripts/openapi_gen/generate.py', '--service-dir', str(service)],
            cwd=work,
            check=True,
        )
        if args.build:
            subprocess.run(
                ['make', 'ci-build-debug', 'CI_BUILD_SERVICES=smoke_00'], cwd=work, check=True
            )
            subprocess.run(
                [
                    'make',
                    'ci-test-debug',
                    'CI_SERVICES=smoke_00',
                    f'CI_RESULTS_DIR={tmp}/results/feature-change',
                ],
                cwd=work,
                check=True,
            )
