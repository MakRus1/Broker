#!/usr/bin/env python3
"""Сервисы определяются наличием services/<name>/service.yaml."""

import argparse
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def validate_names(names):
    names = tuple(sorted(names))
    if not names or any(
        not re.fullmatch(r'[a-z][a-z0-9_]*', name) or len(name) > 40 for name in names
    ):
        raise ValueError('Неверный или пустой каталог сервисов')
    normalized = [name.replace('_', '-') for name in names]
    if len(set(normalized)) != len(names) or set(names) & {
        'all',
        'infrastructure',
        'keycloak',
        'postgres',
        'grafana',
        'prometheus',
        'loki',
        'tempo',
        'collector',
    }:
        raise ValueError('Повторяющееся или зарезервированное имя сервиса')
    return names


def discover(root=ROOT):
    directory = root / 'services'
    paths = list(directory.glob('*/service.yaml'))
    if any(path.resolve().parent.parent != directory.resolve() for path in paths):
        raise ValueError('Сервис выходит за каталог services')
    return validate_names(path.parent.name for path in paths)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plain', action='store_true')
    args = parser.parse_args()
    print(' '.join(discover()) if args.plain else json.dumps(discover()))
