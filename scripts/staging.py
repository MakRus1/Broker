#!/usr/bin/env python3
"""Подготовка и установка личного K3s staging с явным выбором контекста."""

import argparse
import base64
import json
from pathlib import Path
import subprocess

import yaml

from staging_manifest import ROOT, SERVICES, render


def apply(directory, context):
    plan = json.loads((directory / 'plan.json').read_text())
    kubectl = ['kubectl', '--context', context]

    def run(*args):
        subprocess.run([*kubectl, *args], check=True)

    version = subprocess.run(
        [*kubectl, 'version', '-o', 'json'], check=True, text=True, capture_output=True
    )
    actual = json.loads(version.stdout)['serverVersion']['gitVersion']
    if actual != plan['kubernetes_version']:
        raise ValueError(f"Кластер {actual} не соответствует {plan['kubernetes_version']}")
    previous = subprocess.run(
        [
            *kubectl,
            '-n',
            plan['namespace'],
            'get',
            'secret',
            'database',
            '--ignore-not-found',
            '-o',
            'json',
        ],
        check=True,
        text=True,
        capture_output=True,
    )
    if previous.stdout.strip():
        old_password = base64.b64decode(json.loads(previous.stdout)['data']['password']).decode()
        database_secret = next(
            obj
            for obj in yaml.safe_load_all((directory / 'infrastructure.yaml').read_text())
            if obj['kind'] == 'Secret' and obj['metadata']['name'] == 'database'
        )
        if old_password != base64.b64decode(database_secret['data']['password']).decode():
            raise ValueError(
                'Пароль существующей БД отличается: восстановите credentials.json; автоматическая ротация запрещена'
            )
    run(
        'apply',
        '--server-side',
        '--field-manager=broker-staging',
        '--force-conflicts',
        '-f',
        str(directory / 'namespace.yaml'),
    )
    run(
        'apply',
        '--server-side',
        '--field-manager=broker-staging',
        '--force-conflicts',
        '-f',
        str(directory / 'infrastructure.yaml'),
    )
    ns = ['-n', plan['namespace']]
    run(*ns, 'rollout', 'status', 'statefulset/postgres', '--timeout=300s')
    run(*ns, 'rollout', 'status', 'deployment/keycloak', '--timeout=300s')
    run(
        'apply',
        '--server-side',
        '--field-manager=broker-staging',
        '--force-conflicts',
        '-f',
        str(directory / 'migrations.yaml'),
    )
    for name in plan['jobs']:
        run(*ns, 'wait', '--for=condition=complete', f'job/{name}', '--timeout=180s')
    run(
        'apply',
        '--server-side',
        '--field-manager=broker-staging',
        '--force-conflicts',
        '-f',
        str(directory / 'services.yaml'),
    )
    for name in SERVICES:
        run(*ns, 'rollout', 'status', f"deployment/{name.replace('_', '-')}", '--timeout=300s')
    run(*ns, 'get', 'pods,pvc')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    render_parser = commands.add_parser('render')
    render_parser.add_argument('--release', type=Path, required=True)
    render_parser.add_argument('--output', type=Path, default=ROOT / '.staging')
    apply_parser = commands.add_parser('apply')
    apply_parser.add_argument('--output', type=Path, default=ROOT / '.staging')
    apply_parser.add_argument('--context', required=True)
    args = parser.parse_args()
    if args.command == 'render':
        release = json.loads(args.release.read_text())
        revision = subprocess.run(
            ['git', '-C', str(ROOT), 'rev-parse', 'HEAD'],
            check=True,
            text=True,
            capture_output=True,
        ).stdout.strip()
        if release.get('revision') != revision:
            raise ValueError('Checkout репозитория не соответствует revision в release.json')
        render(release, args.output)
        print(f'Манифесты подготовлены: {args.output}. Каталог содержит секреты; не коммитьте его.')
    else:
        apply(args.output, args.context)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, subprocess.CalledProcessError) as exc:
        raise SystemExit(str(exc))
