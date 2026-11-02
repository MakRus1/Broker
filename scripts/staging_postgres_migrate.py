#!/usr/bin/env python3
"""Однократный перенос общей staging-БД в отдельные CNPG-кластеры без удаления исходного PVC."""

import argparse
from copy import deepcopy
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import tempfile

import yaml

from staging_backup import backup, verify_cluster
from staging_manifest import ROOT, private_write
from staging_postgres import cluster_name, install, wait


def import_manifest(objects, owners):
    result = deepcopy(objects)
    for obj in result:
        if obj['kind'] != 'Cluster':
            continue
        owner = obj['metadata']['labels']['broker/database']
        if owner not in owners:
            raise ValueError('Неизвестный владелец БД')
        obj['spec']['bootstrap']['initdb']['import'] = {
            'type': 'microservice',
            'databases': [owner],
            'source': {'externalCluster': 'legacy'},
        }
        obj['spec']['externalClusters'] = [
            {
                'name': 'legacy',
                'connectionParameters': {
                    'host': 'postgres',
                    'user': 'broker',
                    'dbname': 'postgres',
                },
                'password': {'name': 'database', 'key': 'password'},
            }
        ]
    return result


def migrate(directory, context, backup_root):
    plan = json.loads((directory / 'plan.json').read_text())
    kubectl = ['kubectl', '--context', context]
    ns = [*kubectl, '-n', plan['namespace']]
    # Не перезаписываем уже импортированные или используемые целевые БД при повторном запуске.
    install(ROOT, kubectl, environment=plan['environment'])
    existing = json.loads(
        subprocess.check_output(
            [*ns, 'get', 'clusters.postgresql.cnpg.io', '-o', 'json'], text=True
        )
    )['items']
    if existing:
        raise ValueError(
            'CNPG-кластеры уже существуют. Продолжение переноса требует проверки оператором; повторный импорт запрещён'
        )
    source = json.loads(
        subprocess.check_output(
            [*ns, 'get', 'statefulset', 'postgres', '-o', 'json'], text=True
        )
    )
    if source['spec'].get('replicas', 1) != 1:
        raise ValueError('Исходный PostgreSQL должен работать в одном экземпляре')
    deployments = json.loads(
        subprocess.check_output([*ns, 'get', 'deployments', '-o', 'json'], text=True)
    )['items']
    services = json.loads((directory / 'release.json').read_text())['images']
    names = {name.replace('_', '-') for name in services} | {'keycloak'}
    selected = {
        obj['metadata']['name']: obj['spec'].get('replicas', 1)
        for obj in deployments
        if obj['metadata']['name'] in names
    }
    if not selected:
        raise ValueError('Не найдены приложения для остановки записи')
    state = {'phase': 'stopping', 'replicas': selected, 'namespace': plan['namespace']}
    private_write(directory / 'postgres-migration.json', json.dumps(state, indent=2) + '\n')
    for name in selected:
        subprocess.run([*ns, 'scale', 'deployment/' + name, '--replicas=0'], check=True)
    for name in selected:
        subprocess.run(
            [*ns, 'rollout', 'status', 'deployment/' + name, '--timeout=300s'], check=True
        )
    # Копия после остановки всех писателей; её восстановление проверяется до импорта.
    snapshot = backup_root / (
        'postgres-migration-' + datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    )
    backup(snapshot, context, namespace=plan['namespace'])
    image = json.loads((ROOT / 'tools/local/images.json').read_text())['images']['postgres']
    verify_cluster(snapshot, context, plan['namespace'], image)
    state.update(phase='importing', backup=str(snapshot))
    private_write(directory / 'postgres-migration.json', json.dumps(state, indent=2) + '\n')
    objects = list(yaml.safe_load_all((directory / 'databases.yaml').read_text()))
    imports = import_manifest(objects, plan['database_owners'])
    with tempfile.TemporaryDirectory(prefix='broker-pg-import-') as temporary:
        path = Path(temporary) / 'databases.yaml'
        private_write(path, yaml.safe_dump_all(imports, sort_keys=False))
        subprocess.run(
            [
                *kubectl,
                'apply',
                '--server-side',
                '--field-manager=broker-staging',
                '-f',
                str(path),
            ],
            check=True,
        )
    wait(ns, plan['database_owners'])
    subprocess.run([*ns, 'scale', 'statefulset/postgres', '--replicas=0'], check=True)
    subprocess.run(
        [*ns, 'wait', '--for=delete', 'pod/postgres-0', '--timeout=300s'], check=True
    )
    state['phase'] = 'imported'
    private_write(directory / 'postgres-migration.json', json.dumps(state, indent=2) + '\n')
    print(
        'Базы импортированы; исходный PVC сохранён. Выполните Deploy all для переключения приложений. При ошибке приложения остаются остановлены; состояние в postgres-migration.json.'
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / '.staging')
    parser.add_argument('--context', required=True)
    parser.add_argument('--backup-root', type=Path, required=True)
    args = parser.parse_args()
    migrate(args.output.resolve(), args.context, args.backup_root.expanduser().resolve())


if __name__ == '__main__':
    main()
