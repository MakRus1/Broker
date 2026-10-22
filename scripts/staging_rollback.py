#!/usr/bin/env python3
"""История и откат одного сервиса; схема и данные БД не изменяются."""

import argparse
from datetime import datetime, timezone
import fcntl
import json
import os
from pathlib import Path
import subprocess
import sys
import uuid

from service_catalog import discover
from service_history import (
    capture,
    candidates,
    choose,
    check_references,
    fingerprint,
    validate_target,
)
from staging_deploy import deployment_state, write_json

ROOT = Path(__file__).resolve().parents[1]


def describe(snapshot):
    deployment = next(obj for obj in snapshot['objects'] if obj['kind'] == 'Deployment')
    return {
        'revision': deployment['spec']['template']['metadata']
        .get('annotations', {})
        .get('broker/revision'),
        'image': deployment['spec']['template']['spec']['containers'][0]['image'],
        'healthy': snapshot['healthy'],
    }


def execute(root, context, namespace, service, *, target='', apply=False, history_only=False):
    services = discover(root)
    if service not in services:
        raise ValueError('Нет такого сервиса в установленном каталоге')
    os.environ.setdefault('KUBECONFIG', str(Path.home() / '.kube/config'))
    lock_path = Path.home() / '.cache/broker-staging/deploy.lock'
    lock_path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    with lock_path.open('a') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise ValueError('Другой staging-деплой или откат уже выполняется') from None
        kubectl = ['kubectl', '--context', context, '-n', namespace]

        def run(command, **kwargs):
            return subprocess.run(command, pass_fds=(lock.fileno(),), **kwargs)

        current = capture(kubectl, services, run=run)
        if service not in current:
            raise ValueError('У сервиса нет Deployment и Service в кластере')
        history = root / '.staging/releases'
        if history_only:
            rows = [
                {'target': identifier, **describe(snapshot)}
                for identifier, snapshot in candidates(history, service)
            ]
            result = {'service': service, 'current': describe(current[service]), 'history': rows}
            report(result)
            return result
        identifier, snapshot = choose(history, service, current[service], target)
        validate_target(snapshot, service, namespace)
        old_service = next(obj for obj in snapshot['objects'] if obj['kind'] == 'Service')
        current_service = next(
            obj for obj in current[service]['objects'] if obj['kind'] == 'Service'
        )
        if old_service['spec'].get('clusterIP') != current_service['spec'].get('clusterIP'):
            raise ValueError(
                'Service был пересоздан с другим ClusterIP; автоматический откат запрещён'
            )
        check_references(kubectl, snapshot, run=run)
        result = {
            'service': service,
            'target': identifier,
            'before': describe(current[service]),
            'restore': describe(snapshot),
        }
        report(result)
        if not apply:
            print('Предпросмотр: Kubernetes и БД не изменены.')
            return result
        if fingerprint(snapshot) == fingerprint(current[service]):
            if not current[service]['healthy']:
                raise ValueError(
                    'Это состояние уже установлено, но сервис не готов; выберите другой снимок'
                )
            print('Это состояние уже установлено; изменений нет.')
            return result
        record = history / (
            datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S') + '-' + uuid.uuid4().hex[:8]
        )
        record.mkdir(mode=0o700, parents=True)
        write_json(record / 'operation.json', {'operation': 'rollback', **result})
        write_json(record / 'selection.json', {'services': [service], 'infrastructure': False})
        write_json(record / 'before-services.json', current)
        try:
            from permission_catalog import (
                restore as restore_permissions,
                finish as finish_permissions,
            )

            # Каталоги сохраняются по digest образа, исходники старого релиза не нужны.
            restore_permissions(kubectl, service, describe(snapshot)['image'])
            # В apply передаются только два объекта одного сервиса, без Job/SQL/инфраструктуры.
            run(
                [
                    *kubectl,
                    'apply',
                    '--server-side',
                    '--field-manager=broker-staging',
                    '--force-conflicts',
                    '-f',
                    '-',
                ],
                input=json.dumps(
                    {'apiVersion': 'v1', 'kind': 'List', 'items': snapshot['objects']}
                ),
                text=True,
                check=True,
            )
            run(
                [
                    *kubectl,
                    'rollout',
                    'status',
                    f"deployment/{service.replace('_', '-')}",
                    '--timeout=300s',
                ],
                check=True,
            )
            finish_permissions(kubectl, service)
            run(
                [
                    str(root / '.venv-gen/bin/python'),
                    str(root / 'tools/staging/check.py'),
                    '--context',
                    context,
                    '--namespace',
                    namespace,
                ],
                check=True,
            )
        except BaseException:
            (record / 'status').write_text('failed\n')
            raise
        finally:
            try:
                actual = capture(kubectl, services, run=run)
                write_json(record / 'after-services.json', actual)
                deployments = json.loads(
                    run(
                        [*kubectl, 'get', 'deployments', '-o', 'json'],
                        check=True,
                        text=True,
                        capture_output=True,
                    ).stdout
                )
                write_json(
                    root / '.staging/deployed.json',
                    {
                        'record': record.name,
                        'operation': 'rollback',
                        'services': deployment_state(deployments['items'], services),
                    },
                )
            except Exception:
                (record / 'status').write_text('failed\n')
                raise
        (record / 'status').write_text('success\n')
        print(f'Откат {service} завершён. БД и миграции не изменялись.')
        return result


def report(result):
    text = json.dumps(result, indent=2, ensure_ascii=False)
    print(text, flush=True)
    if os.environ.get('GITHUB_STEP_SUMMARY'):
        with open(os.environ['GITHUB_STEP_SUMMARY'], 'a') as summary:
            summary.write('### История / откат сервиса\n```json\n' + text + '\n```\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('history', 'rollback'))
    parser.add_argument('--context', required=True)
    parser.add_argument('--namespace', default='broker-staging')
    parser.add_argument('--service', required=True)
    parser.add_argument('--target', default='')
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    if args.command == 'history' and (args.apply or args.target):
        parser.error('history не принимает --apply или --target')
    execute(
        ROOT,
        args.context,
        args.namespace,
        args.service,
        target=args.target,
        apply=args.apply,
        history_only=args.command == 'history',
    )


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as exc:
        raise SystemExit(str(exc))
