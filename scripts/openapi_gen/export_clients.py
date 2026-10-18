#!/usr/bin/env python3
"""Экспорт выбранных HTTP-контрактов сервиса с проверкой актуальности без записи."""
import argparse
from copy import deepcopy
from pathlib import Path
import re
import sys
import yaml

ROOT = Path(__file__).resolve().parents[2]


class SpecDumper(yaml.SafeDumper):
    def ignore_aliases(self, data):
        # Повторяем определения явно, чтобы экспорт оставался читаемым.
        return True


def matches(path, pattern):
    if not isinstance(pattern, str) or not pattern.startswith('/') or '?' in pattern or '[' in pattern or '\\' in pattern:
        raise ValueError(f'Неверный glob URL: {pattern!r}')
    if any('**' in segment and segment != '**' for segment in pattern.split('/')):
        raise ValueError('** допускается только отдельным сегментом URL')
    if path == '/' and pattern == '/**':
        return True
    expression = ''.join('(?:/[^/]+)*' if segment == '**' else '/' + re.escape(segment).replace(r'\*', '[^/]*') for segment in pattern.strip('/').split('/'))
    return re.fullmatch(expression, path) is not None


def plan(service):
    from generate import UniqueLoader, public_schema_dir
    from model import parse_spec, identifier
    from documents import Documents, read_files
    identifier(service.name)
    if service.parent.name != "services":
        raise ValueError("Экспорт ожидает каталог services/<service>")
    policy = yaml.load((service / 'client-api.yaml').read_text(), Loader=UniqueLoader)
    if not isinstance(policy, dict) or set(policy) - {'version', 'include', 'exclude'} or type(policy.get('version')) is not int or policy.get('version') != 1 or (('include' in policy) == ('exclude' in policy)):
        raise ValueError('client-api.yaml: version: 1 и ровно один список include либо exclude')
    mode = 'include' if 'include' in policy else 'exclude'
    patterns = policy[mode]
    if not isinstance(patterns, list) or any(not isinstance(item, str) for item in patterns):
        raise ValueError('Правила экспорта должны быть списком glob URL')
    for pattern in patterns:
        matches('/', pattern)
    destination = public_schema_dir(service)
    source = service / 'docs/api'
    boundary = service / 'docs'
    files = read_files([source, boundary / 'components'], boundary)
    documents = Documents(files, boundary, boundary / 'components', UniqueLoader)
    documents.validate_components()
    def output_path(path):
        path = path.resolve()
        if path.is_relative_to((boundary / 'components').resolve()):
            return destination / 'components' / path.relative_to((boundary / 'components').resolve())
        if path.is_relative_to((source / 'components').resolve()):
            raise ValueError('docs/api/components зарезервирован; используйте docs/components')
        return destination / path.relative_to(source.resolve())
    result = {}
    for path in sorted(files):
        if not path.is_relative_to(source):
            continue
        data = documents.document(path)
        parse_spec(documents.bundle(path))
        selected = {url: operation for url, operation in data['paths'].items()
                    if any(matches(url, pattern) for pattern in patterns) == (mode == 'include')}
        if not selected:
            continue
        exported = deepcopy(data)
        exported['paths'] = selected
        copied = {path: exported}
        copied.update({dep: documents.document(dep) for dep in documents.dependencies(exported, path)})
        for origin, document in copied.items():
            rewritten = documents.rewrite(document, origin, output_path)
            result[output_path(origin)] = yaml.dump(rewritten, Dumper=SpecDumper, allow_unicode=True, sort_keys=False)
    if not result:
        raise ValueError('Правила экспорта не выбрали ни одной ручки')
    return destination, result


def export(service, check=False):
    destination, expected = plan(service)
    if destination.is_symlink() or any(parent.is_symlink() for parent in [destination.parent, destination.parent.parent]):
        raise ValueError('Каталог экспорта не может быть symlink')
    for path in expected:
        if not path.resolve().is_relative_to(destination.resolve()):
            raise ValueError('Файл экспорта выходит за каталог клиентских схем')
    existing = {path for path in destination.rglob('*') if path.is_file() and path.suffix in {'.yaml', '.yml', '.json'}}
    changed = {path: content for path, content in expected.items() if not path.exists() or path.read_text() != content}
    stale = existing - expected.keys()
    if check:
        if changed or stale:
            raise ValueError(f'{service.name}: клиентские спеки устарели; выполните make export-client-specs SERVICE={service.name}')
        return
    for path, content in changed.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        if path.is_symlink():
            raise ValueError('Файл экспорта не может быть symlink')
        path.write_text(content)
    for path in stale:
        path.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--service-dir', type=Path)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    services = [args.service_dir.resolve()] if args.service_dir else sorted((ROOT / 'services').iterdir())
    for service in services:
        if service.is_dir() and not service.name.startswith('.'):
            export(service, args.check)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, yaml.YAMLError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
