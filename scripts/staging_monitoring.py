"""Обновление целей Prometheus и дашбордов без повторной установки инфраструктуры."""

import argparse
import json
from pathlib import Path
import subprocess
import yaml

NAMES = ('prometheus', 'grafana')


def run(command, **kwargs):
    return subprocess.run(command, check=True, text=True, **kwargs)


def refresh(directory, context, namespace):
    manifest = directory / 'monitoring.yaml'
    objects = [obj for obj in yaml.safe_load_all(manifest.read_text()) if obj]
    maps = {obj['metadata']['name']: obj for obj in objects if obj['kind'] == 'ConfigMap'}
    deployments = {obj['metadata']['name']: obj for obj in objects if obj['kind'] == 'Deployment'}
    prefix = ['kubectl', '--context', context, '-n', namespace]
    updates = []
    for name in NAMES:
        expected = deployments[name]['spec']['template']['spec']['volumes']
        current = json.loads(
            run([*prefix, 'get', 'deployment', name, '-o', 'json'], capture_output=True).stdout
        )
        existing = {
            volume['name']: volume.get('configMap', {}).get('name')
            for volume in current['spec']['template']['spec']['volumes']
        }
        changed = [
            volume
            for volume in expected
            if 'configMap' in volume and existing.get(volume['name']) != volume['configMap']['name']
        ]
        for volume in changed:
            config = maps[volume['configMap']['name']]
            if config['metadata'].get('namespace') != namespace:
                raise ValueError('Конфигурация мониторинга относится к другому namespace')
        if changed:
            updates.append((name, changed))
    for name, volumes in updates:
        configs = [maps[volume['configMap']['name']] for volume in volumes]
        run(
            [
                *prefix,
                'apply',
                '--server-side',
                '--field-manager=broker-staging',
                '--force-conflicts',
                '-f',
                '-',
            ],
            input=yaml.safe_dump_all(configs, sort_keys=False),
        )
        # Меняем только ссылки на конфигурацию. Образы, ресурсы, PVC, TLS и секреты
        # остаются у текущего deployment и не берутся из нового релизного манифеста.
        patch = {'spec': {'template': {'spec': {'volumes': volumes}}}}
        run(
            [
                *prefix,
                'patch',
                'deployment',
                name,
                '--type=strategic',
                '--field-manager=broker-staging',
                '-p',
                json.dumps(patch),
            ]
        )
    for name, _ in updates:
        run([*prefix, 'rollout', 'status', f'deployment/{name}', '--timeout=300s'])
    print('Цели Prometheus и дашборды Grafana актуальны')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', required=True, type=Path)
    parser.add_argument('--context', required=True)
    args = parser.parse_args()
    plan = json.loads((args.directory / 'plan.json').read_text())
    if not plan.get('lan'):
        parser.error('Мониторинг LAN не включён')
    refresh(args.directory, args.context, plan['namespace'])
