#!/usr/bin/env python3
"""Сборка образов из закреплённого окружения; release.json содержит digest каждого сервиса."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SERVICES = ('configs', 'broker', 'admin_bff')


def build(revision, *, push=False, platform='linux/amd64', output=ROOT / '.staging/release.json'):
    if len(revision) != 40 or any(c not in '0123456789abcdef' for c in revision):
        raise ValueError('Нужен полный git SHA')
    dev = json.loads((ROOT / 'tools/container-images.json').read_text())['images']['dev']
    node = json.loads((ROOT / 'tools/local/images.json').read_text())['images']['node']
    release = {'revision': revision, 'images': {}}
    with tempfile.TemporaryDirectory() as directory:
        for index, name in enumerate(SERVICES):
            image = f"ghcr.io/makrus1/broker-{name.replace('_', '-')}:{revision}"
            metadata = Path(directory) / f'{name}.json'
            cache = []
            if os.environ.get('GITHUB_ACTIONS') == 'true':
                scope = 'broker-services-' + platform.replace('/', '-')
                cache = ['--cache-from', f'type=gha,version=2,scope={scope}']
                if index == len(SERVICES) - 1:
                    cache += ['--cache-to', f'type=gha,version=2,scope={scope},mode=max']
            subprocess.run(
                [
                    'docker',
                    'buildx',
                    'build',
                    '--file',
                    str(ROOT / 'tools/deploy/Dockerfile'),
                    '--platform',
                    platform,
                    '--target',
                    'runtime',
                    '--build-arg',
                    f'DEV_IMAGE={dev}',
                    '--build-arg',
                    f'NODE_IMAGE={node}',
                    '--build-arg',
                    f'SERVICE={name}',
                    '--build-arg',
                    f'REVISION={revision}',
                    '--tag',
                    image,
                    '--metadata-file',
                    str(metadata),
                    *cache,
                    '--push' if push else '--load',
                    str(ROOT),
                ],
                check=True,
            )
            digest = json.loads(metadata.read_text())['containerimage.digest']
            release['images'][name] = image.split(':')[0] + '@' + digest
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(release, indent=2) + '\n')
    print(f'Release: {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision', required=True)
    parser.add_argument('--push', action='store_true')
    parser.add_argument('--platform', default='linux/amd64')
    parser.add_argument('--output', type=Path, default=ROOT / '.staging/release.json')
    args = parser.parse_args()
    build(args.revision, push=args.push, platform=args.platform, output=args.output)
