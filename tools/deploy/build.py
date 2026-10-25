#!/usr/bin/env python3
"""Сборка образов из закреплённого окружения; release.json содержит digest каждого сервиса."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from service_catalog import discover
from staging_deploy import validate_release

SERVICES = discover()


def selected_services(revision, services, base):
    if base is None:
        return list(services), {name: ['Нет базового релиза — полная сборка'] for name in services}
    validate_release(base)
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    if head != revision:
        raise ValueError('Выборочная сборка требует checkout указанной revision')
    subprocess.run(
        ['git', 'merge-base', '--is-ancestor', base['revision'], revision], cwd=ROOT, check=True
    )
    paths = (
        subprocess.check_output(
            ['git', 'diff', '--name-only', '-z', base['revision'], revision], cwd=ROOT
        )
        .decode()
        .split('\0')
    )
    packaging = (
        '.github/workflows/services.yml',
        'tools/deploy/',
        '.dockerignore',
        'tools/container-images.json',
        'tools/local/images.json',
    )
    if any(path == prefix or path.startswith(prefix) for path in paths for prefix in packaging):
        return list(services), {
            name: ['Изменилась упаковка или окружение образов'] for name in services
        }
    impact = json.loads(
        subprocess.check_output(
            [
                sys.executable,
                str(ROOT / 'scripts/affected-services.py'),
                base['revision'],
                '--json',
            ],
            cwd=ROOT,
            text=True,
        )
    )
    reasons = impact['reasons']
    for app, service in [('admin', 'admin_bff'), ('web', 'web_bff')]:
        if any(path.startswith(f'frontend/{app}/') for path in paths):
            reasons.setdefault(service, []).append(f'Изменён фронтенд {app}')
    for service in services:
        if service not in base['images']:
            reasons.setdefault(service, []).append('Новый сервис')
    return sorted(set(reasons) & set(services)), reasons


def build(
    revision,
    *,
    push=False,
    platform='linux/amd64',
    output=ROOT / '.staging/release.json',
    base=None,
):
    if len(revision) != 40 or any(c not in '0123456789abcdef' for c in revision):
        raise ValueError('Нужен полный git SHA')
    if base is not None and platform != 'linux/amd64':
        raise ValueError(
            'Наследование опубликованного релиза поддерживается только для linux/amd64'
        )
    dev = json.loads((ROOT / 'tools/container-images.json').read_text())['images']['dev']
    node = json.loads((ROOT / 'tools/local/images.json').read_text())['images']['node']
    services = discover(ROOT)
    selected, reasons = selected_services(revision, services, base)
    release = {
        'revision': revision,
        'images': {
            name: base['images'][name] for name in services if base and name in base['images']
        },
    }
    summary = '### План сборки релизных образов\n\n'
    summary += f"Базовый релиз: `{base['revision']}`.\n\n" if base else 'Базового релиза нет.\n\n'
    summary += '| Сервис | Действие | Причина |\n| --- | --- | --- |\n'
    for name in services:
        action = 'Собрать' if name in selected else 'Сохранить digest'
        reason = '; '.join(reasons.get(name, [])) or 'Образ не затронут'
        summary += f'| `{name}` | {action} | {reason} |\n'
    print(summary, flush=True)
    if target := os.environ.get('GITHUB_STEP_SUMMARY'):
        with open(target, 'a') as stream:
            stream.write(summary + '\n')
    with tempfile.TemporaryDirectory() as directory:
        for name in selected:
            image = f"ghcr.io/makrus1/broker-{name.replace('_', '-')}:{revision}"
            metadata = Path(directory) / f'{name}.json'
            cache = []
            if os.environ.get('GITHUB_ACTIONS') == 'true':
                scope = f"broker-service-{name}-{platform.replace('/', '-')}"
                cache = ['--cache-from', f'type=gha,version=2,scope={scope}']
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
                    'ASSETS_STAGE='
                    + {'admin_bff': 'frontend-admin', 'web_bff': 'frontend-web'}.get(
                        name, 'assets-none'
                    ),
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
    validate_release(release)
    if set(release['images']) != set(services):
        raise ValueError('Релиз не содержит все сервисы')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(release, indent=2) + '\n')
    print(f'Release: {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision', required=True)
    parser.add_argument('--push', action='store_true')
    parser.add_argument('--platform', default='linux/amd64')
    parser.add_argument('--output', type=Path, default=ROOT / '.staging/release.json')
    parser.add_argument('--base-release', type=Path)
    args = parser.parse_args()
    build(
        args.revision,
        push=args.push,
        platform=args.platform,
        output=args.output,
        base=json.loads(args.base_release.read_text()) if args.base_release else None,
    )
