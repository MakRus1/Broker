"""Регистрация каталога релиза и сверка с работающими образами без удаления назначений."""

import json
from pathlib import Path
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent / 'openapi_gen'))
from permissions import catalog
import yaml


class UniqueLoader(yaml.SafeLoader):
    pass


def unique_mapping(loader, node, deep=False):
    result = {}
    for key, value in loader.construct_pairs(node, deep=deep):
        if key in result:
            raise ValueError('Повторяющийся ключ каталога прав: ' + str(key))
        result[key] = value
    return result


UniqueLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, unique_mapping)


def load(root):
    return catalog(Path(root), UniqueLoader)


def release_entries(owner, entries):
    return [dict(code=code, name=value['name'], group=value['group'], scope=value['scope'],
                 dependencies=value['requires'])
            for code, value in sorted(entries.items()) if value['owner'] == owner]


def register(base_url, token_url, secret, action, body):
    from release_catalog_api import request_json
    access = request_json(token_url, {'grant_type': 'client_credentials',
        'client_id': 'permission-publisher', 'client_secret': secret}, form=True)
    return request_json(base_url + '/api/internal/permissions/releases/' + action,
                        body, access['access_token'])


def call_cluster(kubectl, secret, action, body):
    from release_catalog_api import forward_service
    with forward_service(kubectl, 'keycloak') as keycloak, forward_service(kubectl, 'organizations') as organizations:
        return register(organizations, keycloak + '/realms/broker/protocol/openid-connect/token',
                        secret, action, body)


def live_images(kubectl, service):
    result = subprocess.run(
        [*kubectl, 'get', 'pods', '-o', 'json'], capture_output=True, text=True, check=True
    )
    images = set()
    for pod in json.loads(result.stdout)['items']:
        if pod['metadata'].get('labels', {}).get('app.kubernetes.io/name') != service:
            continue
        if pod.get('status', {}).get('phase') in {'Succeeded', 'Failed'}:
            continue
        images.update(
            container['image']
            for container in pod['spec']['containers']
            if container['name'] == service.replace('_', '-')
        )
    return images


def prepare(kubectl, owner, image, entries, secret):
    call_cluster(kubectl, secret, 'upsert', dict(owner=owner, version=image,
                                             entries=release_entries(owner, entries)))


def finish(kubectl, owner, secret):
    # rollout status не гарантирует завершение terminating-подов: ждём их дренирования.
    for attempt in range(46):
        result = subprocess.run(
            [*kubectl, 'get', 'pods', '-o', 'json'], capture_output=True, text=True, check=True
        )
        draining = [
            pod
            for pod in json.loads(result.stdout)['items']
            if pod['metadata'].get('labels', {}).get('app.kubernetes.io/name') == owner
            and pod['metadata'].get('deletionTimestamp')
        ]
        if not draining:
            versions = sorted(live_images(kubectl, owner))
            if not versions:
                raise ValueError('Нельзя сверять каталог без работающих версий: ' + owner)
            call_cluster(kubectl, secret, 'reconcile', dict(owner=owner, versions=versions))
            return
        if attempt != 45:
            time.sleep(2)
    raise RuntimeError('Каталог сохранён: старые поды ещё завершаются: ' + owner)


def restore(kubectl, owner, image, secret):
    call_cluster(kubectl, secret, 'activate', dict(owner=owner, version=image))
