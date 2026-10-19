"""Проверка потребностей сервиса для развёртывания."""

import re
from copy import deepcopy


def validate(value):
    if not isinstance(value, dict) or set(value) - {'replicas', 'resources'}:
        raise ValueError('deployment: поддерживаются replicas/resources')
    replicas = value.get('replicas', 1)
    if type(replicas) is not int or replicas < 1:
        raise ValueError('deployment.replicas: положительное целое число')
    resources = value.get('resources', {})
    if not isinstance(resources, dict) or set(resources) - {'requests', 'limits'}:
        raise ValueError('deployment.resources: поддерживаются requests/limits')
    for kind, quantities in resources.items():
        if not isinstance(quantities, dict) or set(quantities) - {'cpu', 'memory'}:
            raise ValueError(f'deployment.resources.{kind}: поддерживаются cpu/memory')
        for resource, quantity in quantities.items():
            pattern = (
                r'(?:[1-9][0-9]*m|[0-9]+(?:\.[0-9]+)?)'
                if resource == 'cpu'
                else r'[1-9][0-9]*(?:Ki|Mi|Gi)'
            )
            if not isinstance(quantity, str) or not re.fullmatch(pattern, quantity):
                raise ValueError(
                    f'deployment.resources.{kind}.{resource}: неверная величина {quantity!r}'
                )
            if resource == 'cpu' and float(quantity.removesuffix('m')) <= 0:
                raise ValueError('deployment.resources.cpu: значение должно быть больше нуля')
    for resource in ('cpu', 'memory'):
        request = resources.get('requests', {}).get(resource)
        limit = resources.get('limits', {}).get(resource)
        if request and limit and amount(request) > amount(limit):
            raise ValueError(f'deployment.resources: request {resource} больше limit')
    return value


def amount(quantity):
    for suffix, factor in [('Ki', 1024), ('Mi', 1024**2), ('Gi', 1024**3), ('m', 0.001)]:
        if quantity.endswith(suffix):
            return float(quantity[: -len(suffix)]) * factor
    return float(quantity)


def merge(*layers):
    result = {}
    for layer in layers:
        validate(layer)
        for key, value in layer.items():
            if key != 'resources':
                result[key] = value
                continue
            for kind, quantities in value.items():
                result.setdefault('resources', {}).setdefault(kind, {}).update(deepcopy(quantities))
    return validate(result)
