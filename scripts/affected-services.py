#!/usr/bin/env python3
"""Определение затронутых сервисов: ошибки Git и конфигурации не скрываются."""

from pathlib import Path
import subprocess
import json
import re
import sys
import yaml

ROOT = Path(__file__).resolve().parent.parent


def git(*args, check=True):
    return subprocess.run(['git', '-C', str(ROOT), *args], capture_output=True, check=check)


def main():
    config = yaml.safe_load((ROOT / '.github/service-deps.yaml').read_text())
    if not isinstance(config.get('libs'), dict) or not isinstance(config.get('shared_paths'), list):
        raise ValueError('Некорректный service-deps.yaml')
    services = {p.name for p in (ROOT / 'services').iterdir() if p.is_dir()}
    dependencies = {}
    consumers = {name: set() for name in services}
    config_consumers = {}
    for name, item in config['libs'].items():
        entries = item['services']
        if not isinstance(entries, list) or not set(entries) <= services:
            raise ValueError(f'Неизвестный сервис в зависимостях {name}')
        dependencies[name] = entries
    # service.yaml — источник зависимостей новых сервисов; карта остается для старых.
    for service in services:
        manifest = ROOT / 'services' / service / 'service.yaml'
        if not manifest.exists():
            continue
        spec = yaml.safe_load(manifest.read_text())
        if not isinstance(spec, dict):
            raise ValueError(f'Некорректный manifest {manifest}')
        clients = spec.get('clients', {})
        if not isinstance(clients, dict):
            raise ValueError(f'Некорректные clients в {manifest}')
        for alias, client in clients.items():
            if (
                not isinstance(alias, str)
                or not re.fullmatch(r'[a-zA-Z][a-zA-Z0-9_]*', alias)
                or not isinstance(client, dict)
                or set(client) != {'service', 'transport'}
                or client['transport'] != 'http'
                or client['service'] not in services
            ):
                raise ValueError(f'Некорректный клиент {alias} в {manifest}')
            consumers[client['service']].add(service)
            config_consumers.setdefault(
                client['service'].upper() + '_HTTP_CLIENT_CONFIG', set()
            ).add(service)
        for config_name in spec.get('configs', []):
            config_consumers.setdefault(config_name, set()).add(service)
        libraries = spec.get('libraries', ['common'])
        if 'auth' in spec:
            if not isinstance(spec['auth'], dict):
                raise ValueError(f'Некорректная секция auth в {manifest}')
            if 'auth' not in libraries:
                libraries = [*libraries, 'auth']
        if clients and 'configs' not in libraries:
            libraries = [*libraries, 'configs']
        if not isinstance(libraries, list) or any(
            not isinstance(lib, str)
            or not re.fullmatch(r'[a-z][a-z0-9_]*', lib)
            or not (ROOT / 'libs' / lib / 'CMakeLists.txt').is_file()
            for lib in libraries
        ):
            raise ValueError(f'Некорректные libraries в {manifest}')
        for entries in dependencies.values():
            if service in entries:
                entries.remove(service)
        for lib in libraries:
            dependencies.setdefault(lib, []).append(service)
    library_paths = {path.parent.name: path for path in (ROOT / 'libs').glob('*/CMakeLists.txt')}
    library_consumers = {name: set(dependencies.get(name, [])) for name in library_paths}
    changed_graph = True
    while changed_graph:
        changed_graph = False
        for name, path in library_paths.items():
            source = re.sub(r'#[^\n]*', '', path.read_text())
            for dependency in set(re.findall(r'\bbroker_([a-z][a-z0-9_]*)\b', source)) - {name}:
                if dependency in library_consumers:
                    before = len(library_consumers[dependency])
                    library_consumers[dependency].update(library_consumers[name])
                    changed_graph |= len(library_consumers[dependency]) != before
    dependencies.update(library_consumers)
    git('rev-parse', '--is-inside-work-tree')
    base = sys.argv[1] if len(sys.argv) > 1 else 'origin/main'
    if git('rev-parse', '--verify', base + '^{commit}', check=False).returncode:
        print(
            json.dumps(
                {
                    'services': sorted(services),
                    'reasons': {name: ['Базовая ревизия недоступна'] for name in sorted(services)},
                }
            )
            if '--json' in sys.argv
            else 'all'
        )
        return
    # -z сохраняет пробелы и переводы строк в именах; удаленные сервисы не собираются.
    changed = git('diff', '--name-only', '-z', base, 'HEAD').stdout.decode().split('\0')
    schema_consumers = {}
    for path in (ROOT / 'schemas/configs').rglob('*.yaml'):
        spec = yaml.safe_load(path.read_text())
        users = set(config_consumers.get(spec['name'], set())) | ({'configs'} & services)
        schema_consumers[path.relative_to(ROOT).as_posix()] = users
        if context := spec.get('context'):
            context_path = 'schemas/config_contexts/' + context + '.yaml'
            schema_consumers.setdefault(context_path, set()).update(users)
    reasons = select(
        changed, services, dependencies, consumers, config['shared_paths'], schema_consumers
    )
    if '--json' in sys.argv:
        print(json.dumps({'services': sorted(reasons), 'reasons': reasons}, ensure_ascii=False))
    elif reasons:
        print(' '.join(sorted(reasons)))


def matches(path, prefix):
    return path == prefix.rstrip('/') or path.startswith(
        prefix if prefix.endswith('/') else prefix + '/'
    )


def select(paths, services, dependencies, consumers, shared_paths, schema_consumers=None):
    reasons = {}
    public = set()

    def add(names, reason):
        for name in names:
            if name in services:
                reasons.setdefault(name, [])
                if reason not in reasons[name]:
                    reasons[name].append(reason)

    for path in filter(None, paths):
        if path.endswith('.md'):
            continue
        parts = path.split('/')
        if any(matches(path, prefix) for prefix in shared_paths):
            add(services, 'Общая платформа: ' + path)
        elif parts[0] == 'libs' and len(parts) > 1:
            add(dependencies.get(parts[1], services), 'Библиотека: ' + path)
        elif parts[:2] == ['schemas', 'services'] and len(parts) > 2 and parts[2] in services:
            add([parts[2], 'configs'], 'Публичный контракт: ' + path)
            public.add(parts[2])
        elif schema_consumers is not None and path in schema_consumers:
            add(schema_consumers[path], 'Схема конфигурации: ' + path)
        elif parts[0] == 'schemas':
            add(services, 'Общая схема: ' + path)
        elif parts[0] == 'services' and len(parts) > 1 and parts[1] in services:
            if path.endswith('/codegen.lock') or path.endswith('.md') or '/monitoring/' in path:
                continue
            add([parts[1]], 'Код, конфигурация или тесты: ' + path)
            if (
                '/docs/api/' in path
                or '/docs/components/' in path
                or path.endswith('/service.yaml')
            ):
                public.add(parts[1])
    # Только изменение контракта распространяется по HTTP-зависимостям.
    pending, visited = list(public), set(public)
    while pending:
        provider = pending.pop()
        for consumer in consumers[provider]:
            add([consumer], 'Потребитель контракта: ' + provider)
            if consumer not in visited:
                visited.add(consumer)
                pending.append(consumer)
    return reasons


if __name__ == '__main__':
    try:
        main()
    except (
        OSError,
        ValueError,
        KeyError,
        TypeError,
        yaml.YAMLError,
        subprocess.CalledProcessError,
    ) as exc:
        sys.exit(f'Ошибка определения сервисов: {exc}')
