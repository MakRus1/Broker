"""Снимки сервисов для отката образа и конфигурации без миграций."""

import hashlib
import json
from pathlib import Path
import re
import subprocess

SERVICE_NAME = r'[a-z][a-z0-9_]{0,39}'
TARGET = r'[0-9]{8}T[0-9]{6}-[0-9a-f]{8}:(before|after)'


def clean(obj):
    metadata = obj['metadata']
    annotations = {
        key: value
        for key, value in metadata.get('annotations', {}).items()
        if key
        not in (
            'deployment.kubernetes.io/revision',
            'kubectl.kubernetes.io/last-applied-configuration',
        )
    }
    return {
        'apiVersion': obj['apiVersion'],
        'kind': obj['kind'],
        'metadata': {
            'name': metadata['name'],
            'namespace': metadata['namespace'],
            'labels': metadata.get('labels', {}),
            'annotations': annotations,
        },
        'spec': obj['spec'],
    }


def capture(kubectl, services, *, run=None):
    run = run or subprocess.run
    raw = json.loads(
        run(
            [*kubectl, 'get', 'deployments,services', '-o', 'json'],
            check=True,
            text=True,
            capture_output=True,
        ).stdout
    )['items']
    result = {}
    for name in services:
        objects = [
            obj
            for obj in raw
            if obj['metadata']['name'] == name.replace('_', '-')
            and obj['kind'] in ('Deployment', 'Service')
        ]
        if {obj['kind'] for obj in objects} != {'Deployment', 'Service'}:
            continue
        deployment = next(obj for obj in objects if obj['kind'] == 'Deployment')
        status = deployment.get('status', {})
        replicas = deployment['spec'].get('replicas', 1)
        healthy = (
            replicas > 0
            and status.get('readyReplicas', 0) == replicas
            and status.get('updatedReplicas', 0) == replicas
            and status.get('availableReplicas', 0) == replicas
            and status.get('observedGeneration', 0) >= deployment['metadata']['generation']
        )
        result[name] = {
            'healthy': healthy,
            'objects': sorted([clean(obj) for obj in objects], key=lambda obj: obj['kind']),
        }
    return result


def fingerprint(snapshot):
    return hashlib.sha256(json.dumps(snapshot['objects'], sort_keys=True).encode()).hexdigest()


def candidates(history, service):
    if not re.fullmatch(SERVICE_NAME, service):
        raise ValueError('Неверное имя сервиса')
    for record in sorted(history.glob('*'), reverse=True):
        if not record.is_dir():
            continue
        status = (
            (record / 'status').read_text().strip()
            if (record / 'status').exists()
            else 'in-progress'
        )
        # before можно использовать и после частичного сбоя; after — только успешный.
        for phase in ('after', 'before'):
            path = record / f'{phase}-services.json'
            if not path.exists() or (phase == 'after' and status != 'success'):
                continue
            snapshot = json.loads(path.read_text()).get(service)
            if snapshot and snapshot.get('healthy'):
                yield f'{record.name}:{phase}', snapshot


def choose(history, service, current, target=''):
    if target and not re.fullmatch(TARGET, target):
        raise ValueError('Нужен ID из истории вида <record>:before или <record>:after')
    for identifier, snapshot in candidates(history, service):
        if target:
            if identifier == target:
                return identifier, snapshot
        elif fingerprint(snapshot) != fingerprint(current):
            return identifier, snapshot
    raise ValueError('Нет подходящего рабочего снимка: сначала нужен деплой с сохранением истории')


def validate_target(snapshot, service, namespace):
    objects = snapshot.get('objects', [])
    if len(objects) != 2 or {obj.get('kind') for obj in objects} != {'Deployment', 'Service'}:
        raise ValueError('Снимок должен содержать только Deployment и Service')
    for obj in objects:
        metadata = obj['metadata']
        if metadata['name'] != service.replace('_', '-') or metadata['namespace'] != namespace:
            raise ValueError('Снимок принадлежит другому сервису или namespace')
        if metadata.get('labels', {}).get('broker/service') != service:
            raise ValueError('Снимок без метки владельца сервиса')
    deployment = next(obj for obj in objects if obj['kind'] == 'Deployment')
    containers = deployment['spec']['template']['spec']['containers']
    expected = rf"ghcr\.io/makrus1/broker-{service.replace('_', '-')}@sha256:[0-9a-f]{{64}}"
    if len(containers) != 1 or not re.fullmatch(expected, containers[0]['image']):
        raise ValueError('Снимок содержит неподходящий образ')


def check_references(kubectl, snapshot, *, run=None):
    run = run or subprocess.run
    deployment = next(obj for obj in snapshot['objects'] if obj['kind'] == 'Deployment')
    for volume in deployment['spec']['template']['spec'].get('volumes', []):
        for kind, key in (('configMap', 'name'), ('secret', 'secretName')):
            if kind not in volume:
                continue
            name = volume[kind][key]
            if not re.fullmatch(r'[a-z0-9][a-z0-9.-]*', name):
                raise ValueError('Неверная ссылка конфигурации в снимке')
            resource = 'configmap' if kind == 'configMap' else 'secret'
            result = run(
                [*kubectl, 'get', resource, name, '-o', 'jsonpath={.immutable}'],
                check=True,
                text=True,
                capture_output=True,
            )
            if volume['name'] in ('config', 'vars') and result.stdout.strip() != 'true':
                raise ValueError('Сервисная конфигурация должна быть immutable')
