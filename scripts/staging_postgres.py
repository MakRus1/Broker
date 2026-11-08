"""CloudNativePG: манифесты, установка оператора и адрес текущего primary."""

import hashlib
import json
from pathlib import Path
import re
import subprocess


def cluster_name(owner):
    if not re.fullmatch(r'[a-z][a-z0-9_]*', owner):
        raise ValueError('Неверное имя владельца БД')
    return owner.replace('_', '-') + '-postgres'


def host(owner):
    return cluster_name(owner) + '-rw'


def render(owner, profile, namespace, storage_class, image, password):
    name = cluster_name(owner)
    secret = {
        'apiVersion': 'v1',
        'kind': 'Secret',
        'metadata': {
            'name': name + '-credentials',
            'namespace': namespace,
            'labels': {'broker/database': owner},
        },
        'type': 'kubernetes.io/basic-auth',
        'stringData': {'username': 'broker', 'password': password},
    }
    cluster = {
        'apiVersion': 'postgresql.cnpg.io/v1',
        'kind': 'Cluster',
        'metadata': {
            'name': name,
            'namespace': namespace,
            'labels': {'broker/database': owner},
        },
        'spec': {
            'instances': profile['instances'],
            'imageName': image,
            'resources': profile['resources'],
            'storage': {'size': profile['storage'], 'storageClass': storage_class},
            'bootstrap': {
                'initdb': {
                    'database': owner,
                    'owner': 'broker',
                    'secret': {'name': name + '-credentials'},
                }
            },
            'affinity': {
                'enablePodAntiAffinity': True,
                'podAntiAffinityType': 'required' if profile['instances'] > 1 else 'preferred',
            },
        },
    }
    return [secret, cluster]


def install(root, kubectl, *, environment='staging'):
    """Установка сохранённого официального манифеста с проверкой SHA256."""
    lock = json.loads((root / 'tools/deploy/postgres/operator.json').read_text())
    manifest = (root / 'tools/deploy/postgres/operator.yaml').read_bytes()
    if hashlib.sha256(manifest).hexdigest() != lock['sha256']:
        raise ValueError('SHA256 манифеста CloudNativePG не совпал')
    import yaml
    from platform_resources import load

    profile = load(root / 'tools/deploy/postgres/platform.yaml', environment)['application']
    if profile['replicas'] != 1:
        raise ValueError('Оператор PostgreSQL пока развёртывается в одной реплике')
    objects = list(yaml.safe_load_all(manifest))
    for obj in objects:
        if obj['kind'] == 'Deployment' and obj['metadata']['name'] == 'cnpg-controller-manager':
            obj['spec']['replicas'] = profile['replicas']
            obj['spec']['template']['spec']['containers'][0]['resources'] = profile['resources']
    subprocess.run(
        [*kubectl, 'apply', '--server-side', '-f', '-'],
        input=yaml.safe_dump_all(objects, sort_keys=False).encode(),
        check=True,
    )
    subprocess.run(
        [
            *kubectl,
            'wait',
            '--for=condition=Established',
            'crd/clusters.postgresql.cnpg.io',
            '--timeout=120s',
        ],
        check=True,
    )
    subprocess.run(
        [
            *kubectl,
            '-n',
            'cnpg-system',
            'rollout',
            'status',
            'deployment/cnpg-controller-manager',
            '--timeout=300s',
        ],
        check=True,
    )


def primary(kubectl, owner):
    obj = json.loads(
        subprocess.check_output(
            [*kubectl, 'get', 'cluster.postgresql.cnpg.io', cluster_name(owner), '-o', 'json'],
            text=True,
        )
    )
    name = obj.get('status', {}).get('currentPrimary', '')
    if not re.fullmatch(re.escape(cluster_name(owner)) + r'-[1-9][0-9]*', name):
        raise ValueError(f'{owner}: primary ещё не выбран')
    return name


def wait(kubectl, owners):
    for owner in owners:
        subprocess.run(
            [
                *kubectl,
                'wait',
                '--for=condition=Ready',
                'cluster.postgresql.cnpg.io/' + cluster_name(owner),
                '--timeout=600s',
            ],
            check=True,
        )


def check_credentials(kubectl, objects):
    import base64

    for obj in objects:
        if obj['kind'] != 'Secret':
            continue
        name = obj['metadata']['name']
        previous = subprocess.run(
            [*kubectl, 'get', 'secret', name, '--ignore-not-found', '-o', 'json'],
            check=True,
            text=True,
            capture_output=True,
        ).stdout
        if previous.strip():
            existing = base64.b64decode(json.loads(previous)['data']['password']).decode()
            if existing != obj['stringData']['password']:
                raise ValueError(
                    f'Пароль {name} отличается: восстановите сохранённые файлы .staging; ротация запрещена'
                )
