#!/usr/bin/env python3
"""Подготовка и установка личного K3s staging с явным выбором контекста."""

import argparse
import base64
import json
from pathlib import Path
import subprocess
import sys
import tempfile

import yaml

from staging_manifest import ROOT, SERVICES, render


def apply(directory, context, services=None, infrastructure=None):
    if infrastructure is None:
        infrastructure = services is None
    if services is None:
        services = list(SERVICES)
    if len(set(services)) != len(services) or set(services) - set(SERVICES):
        raise ValueError('Неверный или повторяющийся сервис')
    if not services and not infrastructure:
        raise ValueError('Не выбраны сервисы или инфраструктура')
    with tempfile.TemporaryDirectory(prefix='broker-selected-') as temporary:
        paths, jobs = {}, []
        for phase in ('services', 'migrations'):
            objects = list(yaml.safe_load_all((directory / f'{phase}.yaml').read_text()))
            selected = []
            for obj in objects:
                if obj is None:
                    continue
                owner = obj['metadata'].get('labels', {}).get('broker/service')
                if owner not in SERVICES:
                    raise ValueError('Манифест без broker/service: повторите render новой версией')
                if owner in services:
                    selected.append(obj)
                    if obj['kind'] == 'Job':
                        jobs.append(obj['metadata']['name'])
            path = Path(temporary) / f'selected-{phase}.yaml'
            path.write_text(yaml.safe_dump_all(selected, sort_keys=False))
            path.chmod(0o600)
            paths[phase] = path if selected else None
        _apply(directory, context, services, infrastructure, paths, jobs)


def _apply(directory, context, services, infrastructure, paths, jobs):
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
    if infrastructure:
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
    if infrastructure and plan.get('lan'):
        from staging_lan import NAMES, sync_keycloak

        run(
            'apply',
            '--server-side',
            '--field-manager=broker-staging',
            '--force-conflicts',
            '-f',
            str(directory / 'monitoring.yaml'),
        )
        sync_keycloak(context, plan['namespace'], plan['admin_origin'])
        for name in NAMES:
            run(*ns, 'rollout', 'status', f'deployment/{name}', '--timeout=300s')
    if paths['migrations']:
        run(
            'apply',
            '--server-side',
            '--field-manager=broker-staging',
            '--force-conflicts',
            '-f',
            str(paths['migrations']),
        )
    for name in jobs:
        run(*ns, 'wait', '--for=condition=complete', f'job/{name}', '--timeout=180s')
    if paths['services']:
        run(
            'apply',
            '--server-side',
            '--field-manager=broker-staging',
            '--force-conflicts',
            '-f',
            str(paths['services']),
        )
    for name in services:
        run(*ns, 'rollout', 'status', f"deployment/{name.replace('_', '-')}", '--timeout=300s')
    if plan.get('lan'):
        subprocess.run(
            [
                sys.executable,
                str(Path(__file__).resolve().parents[1] / 'tools/staging/check.py'),
                '--context',
                context,
                '--namespace',
                plan['namespace'],
            ],
            check=True,
        )
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
    apply_parser.add_argument('--service', action='append', choices=SERVICES)
    apply_parser.add_argument('--infrastructure', action='store_true')
    apply_parser.add_argument('--infra-only', action='store_true')
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
        if args.infra_only and args.service:
            parser.error('--infra-only несовместим с --service')
        selected = [] if args.infra_only else args.service
        include_infrastructure = args.infrastructure or args.infra_only or args.service is None
        apply(args.output, args.context, selected, include_infrastructure)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, subprocess.CalledProcessError) as exc:
        raise SystemExit(str(exc))
