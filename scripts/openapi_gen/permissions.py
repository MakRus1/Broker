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


def outputs(service, loader):
    root = Path(__file__).resolve().parents[2]
    values = catalog(root, loader)
    if service.resolve().parent == (root / 'services').resolve():
        import yaml

        manifest = yaml.load((service / 'service.yaml').read_text(), Loader=loader)
        for code in manifest.get('auth', {}).get('permissions', {}).values():
            if code != 'authenticated' and code not in values:
                raise ValueError(f'{service.name}: право {code} не объявлено в schemas/permissions')
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
