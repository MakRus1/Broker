#!/usr/bin/env python3
"""Определение затронутых сервисов: ошибки Git и конфигурации не скрываются."""
from pathlib import Path
import subprocess
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
            if not isinstance(alias, str) or not re.fullmatch(r'[a-zA-Z][a-zA-Z0-9_]*', alias) or not isinstance(client, dict) or set(client) != {'service', 'transport'} or client['transport'] != 'http' or client['service'] not in services:
                raise ValueError(f'Некорректный клиент {alias} в {manifest}')
            consumers[client['service']].add(service)
        libraries = spec.get('libraries', ['common'])
        if 'auth' in spec:
            if not isinstance(spec['auth'], dict):
                raise ValueError(f'Некорректная секция auth в {manifest}')
            if 'auth' not in libraries:
                libraries = [*libraries, 'auth']
        if clients and 'configs' not in libraries:
            libraries = [*libraries, 'configs']
        if not isinstance(libraries, list) or any(not isinstance(lib, str) or not re.fullmatch(r'[a-z][a-z0-9_]*', lib) or not (ROOT / 'libs' / lib / 'CMakeLists.txt').is_file() for lib in libraries):
            raise ValueError(f'Некорректные libraries в {manifest}')
        for entries in dependencies.values():
            if service in entries:
                entries.remove(service)
        for lib in libraries:
            dependencies.setdefault(lib, []).append(service)
    git('rev-parse', '--is-inside-work-tree')
    base = sys.argv[1] if len(sys.argv) > 1 else 'origin/main'
    if git('rev-parse', '--verify', base + '^{commit}', check=False).returncode:
        print('all')
        return
    # -z сохраняет пробелы и переводы строк в именах; удаленные сервисы не собираются.
    changed = git('diff', '--name-only', '-z', base, 'HEAD').stdout.decode().split('\0')
    affected = set()
    for path in filter(None, changed):
        if any(path == prefix.rstrip('/') or path.startswith(prefix if prefix.endswith('/') else prefix + '/') for prefix in config['shared_paths']):
            print('all')
            return
        parts = path.split('/')
        if parts[0] == 'libs' and len(parts) > 1:
            if parts[1] not in dependencies:
                print('all')
                return
            affected.update(dependencies[parts[1]])
        elif parts[:2] == ['schemas', 'services'] and len(parts) > 2 and parts[2] in services:
            affected.add(parts[2])
            if 'configs' in services:
                affected.add('configs')
        elif parts[0] == 'schemas':
            print('all')
            return
        elif parts[0] == 'services' and len(parts) > 1 and parts[1] in services:
            affected.add(parts[1])
    # Транзитивное замыкание завершается и при взаимных HTTP-вызовах.
    pending = list(affected)
    while pending:
        for consumer in consumers[pending.pop()]:
            if consumer not in affected:
                affected.add(consumer)
                pending.append(consumer)
    if affected:
        print(' '.join(sorted(affected)))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError, yaml.YAMLError, subprocess.CalledProcessError) as exc:
        sys.exit(f'Ошибка определения сервисов: {exc}')
