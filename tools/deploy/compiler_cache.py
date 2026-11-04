"""Перенос ccache между GitHub Actions и cache mount релизного BuildKit."""

import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import uuid

ROOT = Path(__file__).resolve().parents[2]


def transfer(action, directory, platform='linux/amd64'):
    directory = Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    image = json.loads((ROOT / 'tools/container-images.json').read_text())['images']['dev']
    # Пустой context не отправляет исходники или секреты в служебную сборку.
    with tempfile.TemporaryDirectory() as context:
        command = [
            'docker', 'buildx', 'build', '--file', str(ROOT / 'tools/deploy/CompilerCache.Dockerfile'),
            '--platform', platform, '--build-arg', 'DEV_IMAGE=' + image,
            # Новый RUN при каждом переносе; --no-cache сбрасывает и сам cache mount.
            '--build-arg', 'CACHE_TRANSFER=' + uuid.uuid4().hex, '--target', action,
        ]
        if action == 'restore':
            command += ['--build-context', 'compiler-cache=' + str(directory),
                        '--output', 'type=cacheonly']
        elif action == 'export':
            command += ['--output', 'type=local,dest=' + str(directory)]
        else:
            raise ValueError('Неизвестная операция кэша: ' + action)
        subprocess.run([*command, context], check=True, timeout=240)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['restore', 'export'])
    parser.add_argument('--directory', type=Path, default=ROOT / '.release-ccache')
    parser.add_argument('--platform', default='linux/amd64')
    args = parser.parse_args()
    transfer(args.action, args.directory, args.platform)
