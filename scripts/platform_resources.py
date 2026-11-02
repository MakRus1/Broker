"""Профили приложения и его PostgreSQL: defaults → выбранное окружение."""

from copy import deepcopy
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).parent / 'openapi_gen'))
from deployment import amount, merge
from generate import UniqueLoader
import yaml

COMPONENTS = ('postgres', 'keycloak', 'grafana', 'prometheus', 'loki', 'tempo', 'collector')


def overlay(base, override):
    result = deepcopy(base)
    for key, value in override.items():
        if isinstance(value, dict) and isinstance(result.get(key), dict):
            result[key] = overlay(result[key], value)
        else:
            result[key] = deepcopy(value)
    return result


def validate_resources(resources):
    merge({'resources': resources})
    requests = resources.get('requests', {})
    limits = resources.get('limits', {})
    if not {'cpu', 'memory'} <= requests.keys() or 'memory' not in limits:
        raise ValueError('Нужны requests.cpu/memory и limits.memory')


def validate_profile(value, *, postgres):
    if not isinstance(value, dict) or set(value) != (
        {'application', 'postgres'} if postgres else {'application'}
    ):
        raise ValueError('Ожидается application и postgres только у владельца БД')
    application = merge(value['application'])
    validate_resources(application.get('resources', {}))
    if 'replicas' not in application:
        raise ValueError('application.replicas: укажите число реплик')
    if postgres:
        pg = value['postgres']
        if not isinstance(pg, dict) or set(pg) != {'instances', 'resources', 'storage'}:
            raise ValueError('postgres: ожидаются instances/resources/storage')
        if type(pg['instances']) is not int or pg['instances'] < 1:
            raise ValueError('postgres.instances: положительное целое число')
        validate_resources(pg['resources'])
        if not isinstance(pg['storage'], str) or not re.fullmatch(
            r'[1-9][0-9]*(?:Mi|Gi)', pg['storage']
        ):
            raise ValueError(
                'postgres.storage: положительный размер в Mi/Gi на каждый экземпляр'
            )
    return value


def load(path, environment, *, postgres=False):
    value = yaml.load(path.read_text(), Loader=UniqueLoader)
    if (
        not isinstance(value, dict)
        or set(value) != {'defaults', 'environments'}
        or not isinstance(value['environments'], dict)
    ):
        raise ValueError(f'{path}: ожидаются defaults/environments')
    validate_profile(value['defaults'], postgres=postgres)
    for name, overrides in value['environments'].items():
        if not isinstance(overrides, dict):
            raise ValueError(f'{path}: {name} должен быть словарём')
        validate_profile(overlay(value['defaults'], overrides), postgres=postgres)
    if environment not in value['environments']:
        raise ValueError(f'{path}: нет профиля окружения {environment}')
    return validate_profile(
        overlay(value['defaults'], value['environments'][environment]), postgres=postgres
    )


def infrastructure(root, environment):
    result = {}
    for name in COMPONENTS:
        result[name] = load(
            root / f'tools/deploy/{name}/platform.yaml',
            environment,
            postgres=name == 'keycloak',
        )
        if result[name]['application']['replicas'] != 1:
            raise ValueError(f'{name}: текущая конфигурация приложения требует одну реплику')
    return result


def memory_budget(resources, fraction):
    return max(1, int(amount(resources['limits']['memory']) * fraction / 1024**2))
