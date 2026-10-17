#!/usr/bin/env python3
"""Версия окружения по входным файлам и обновление закрепленных digest GHCR."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent.parent
INPUTS = ['tools/Dockerfile', 'tools/build-userver.sh', 'tools/apt-packages.txt',
          'tools/versions.env', 'tools/python-constraints.txt', 'requirements.txt']
REGISTRY = 'ghcr.io/makrus1/broker-dev'
LOCK = ROOT / 'tools/container-images.json'


def fingerprint():
    digest = hashlib.sha256()
    for relative in sorted(INPUTS):
        digest.update(relative.encode() + b'\0' + (ROOT / relative).read_bytes() + b'\0')
    return digest.hexdigest()


def inspect(tag):
    result = subprocess.check_output(['docker', 'buildx', 'imagetools', 'inspect', tag,
                                     '--format', '{{json .Manifest}}'], text=True)
    manifest = json.loads(result)
    digest = manifest['digest']
    platforms = {(m['platform']['os'], m['platform']['architecture'])
                 for m in manifest.get('manifests', [])}
    if not {('linux', 'amd64'), ('linux', 'arm64')} <= platforms:
        raise ValueError(f'{tag}: отсутствует amd64 или arm64')
    if not re.fullmatch(r'sha256:[0-9a-f]{64}', digest):
        raise ValueError(f'Неверный digest {tag}')
    return REGISTRY + '@' + digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['key', 'check', 'ref', 'lock'])
    parser.add_argument('--variant', choices=['dev', 'sanitize'], default='dev')
    args = parser.parse_args()
    key = fingerprint()
    if args.command == 'key':
        print(key)
        return
    if args.command == 'lock':
        data = {'version': 1, 'inputs_sha256': key,
                'images': {variant: inspect(f'{REGISTRY}:{key}-{variant}')
                           for variant in ('dev', 'sanitize')}}
        LOCK.write_text(json.dumps(data, indent=2) + '\n')
        config = ROOT / '.devcontainer/devcontainer.json'
        devcontainer = json.loads(config.read_text())
        devcontainer.pop('build', None)
        devcontainer['image'] = data['images']['dev']
        config.write_text(json.dumps(devcontainer, indent=2, ensure_ascii=False) + '\n')
        print(LOCK)
        return
    data = json.loads(LOCK.read_text())
    if data.get('version') != 1 or data.get('inputs_sha256') != key:
        raise ValueError('Окружение изменилось: дождитесь Publish environment и выполните make lock-images')
    for variant in ('dev', 'sanitize'):
        if not re.fullmatch(re.escape(REGISTRY) + r'@sha256:[0-9a-f]{64}', data['images'][variant]):
            raise ValueError(f'Неверная ссылка на образ {variant}')
    if args.command == 'ref':
        print(data['images'][args.variant])


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as exc:
        raise SystemExit(str(exc)) from exc
