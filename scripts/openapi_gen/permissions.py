"""Каталог прав по модулям и типизированные имена для C++."""

import json
import re
from pathlib import Path


def catalog(root, loader):
    import yaml

    result = {}
    for path in sorted((root / 'schemas/permissions').glob('*.yaml')):
        document = yaml.load(path.read_text(), Loader=loader)
        if not isinstance(document, dict):
            raise ValueError(f'{path}: ожидается таблица прав')
        for code, value in document.items():
            if not isinstance(code, str) or not re.fullmatch(
                r'[a-z][a-z0-9_]*\.[a-z][a-z0-9_]*', code
            ):
                raise ValueError(f'{path}: неверный код права {code}')
            if code in result or not code.startswith(path.stem + '.'):
                raise ValueError(f'{path}: повтор или неверный владелец права {code}')
            if not isinstance(value, dict) or set(value) != {'name', 'group', 'scope', 'requires'}:
                raise ValueError(f'{path}: право требует name, group, scope, requires')
            if (
                value['scope'] not in {'organization', 'platform', 'service'}
                or any(
                    not isinstance(value[key], str) or not value[key].strip()
                    for key in ('name', 'group')
                )
                or not isinstance(value['requires'], list)
            ):
                raise ValueError(f'{path}: неверное описание {code}')
            result[code] = {**value, 'owner': path.stem}

    def visit(code, stack):
        if code in stack:
            raise ValueError('Цикл зависимостей прав: ' + code)
        for dependency in result[code]['requires']:
            if dependency not in result or result[dependency]['scope'] != result[code]['scope']:
                raise ValueError(f'{code}: неизвестная или несовместимая зависимость {dependency}')
            visit(dependency, stack | {code})

    for code in result:
        visit(code, set())
    return result


def selected(service, loader, root=None):
    """Права сервиса, явно используемые права и их транзитивные зависимости."""
    import yaml

    root = root or Path(__file__).resolve().parents[2]
    values = catalog(root, loader)
    manifest_path = service / 'service.yaml'
    manifest = yaml.load(manifest_path.read_text(), Loader=loader) if manifest_path.exists() else {}
    codes = {code for code, value in values.items() if value['owner'] == service.name}
    codes.update(manifest.get('auth', {}).get('permissions', {}).values())
    for account in manifest.get('service_accounts', {}).values():
        codes.update(account.get('permissions', []))
    codes.discard('authenticated')
    pending = list(codes)
    while pending:
        code = pending.pop()
        if code not in values:
            raise ValueError(f'{service.name}: право {code} не объявлено в schemas/permissions')
        for dependency in values[code]['requires']:
            if dependency not in codes:
                codes.add(dependency)
                pending.append(dependency)
    return {code: values[code] for code in sorted(codes)}


def outputs(service, loader):
    values = selected(service, loader)
    names = set()
    lines = ['#pragma once', '#include <string_view>', 'namespace permissions {']
    for code in values:
        name = code.replace('.', '_').upper()
        if name in names:
            raise ValueError('Коллизия C++ имён прав: ' + code)
        names.add(name)
        lines.append(f'inline constexpr std::string_view {name} = {json.dumps(code)};')
    lines.append('}  // namespace permissions')
    return {'include/permissions/permissions.hpp': '\n'.join(lines) + '\n'}
