#!/usr/bin/env python3
"""Проверка переменных конфигурации до запуска C++ сервиса."""
from pathlib import Path
import re
import sys
import yaml


def check(service):
    text = (service / 'configs/static_config.yaml').read_text()
    config = yaml.safe_load(text)['components_manager']['components']
    refs = set(re.findall(r'(?<![\w$])\$([a-zA-Z][a-zA-Z0-9_-]*)', text))
    for path in sorted((service / 'configs').glob('config_vars*.yaml')):
        values = yaml.safe_load(path.read_text())
        missing = refs - set(values)
        if missing:
            raise ValueError(f'{path}: отсутствуют переменные {sorted(missing)}')
        if 'testing' not in path.name and values.get('is-testing', False):
            raise ValueError(f'{path}: тестовые обработчики включены вне тестового режима')
    if config.get('tests-control', {}).get('load-enabled') != '$is-testing':
        raise ValueError(f'{service}: tests-control должен зависеть от is-testing')
    if config['server']['listener-monitor'].get('address') != '127.0.0.1':
        raise ValueError(f'{service}: служебный listener должен слушать loopback')


if __name__ == '__main__':
    try:
        for directory in Path('services').iterdir():
            if directory.is_dir():
                check(directory)
    except (ValueError, KeyError, OSError) as exc:
        sys.exit(str(exc))
