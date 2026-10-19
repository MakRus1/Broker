#!/usr/bin/env python3
"""Создание сервиса из проверяемого шаблона с откатом регистрации при ошибке."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import yaml
from service_catalog import validate_names

ROOT = Path(__file__).resolve().parent.parent


def _create(name, postgres=False, grpc=False):
    validate_names([name])
    if not re.fullmatch(r'[a-z][a-z0-9_]*', name) or name in {'class', 'template', 'namespace', 'new', 'delete', 'default', 'return', 'operator'}:
        raise ValueError('Имя сервиса: строчные латинские буквы, цифры и _, начиная с буквы; ключевые слова запрещены')
    sys.path.insert(0, str(ROOT / 'scripts/openapi_gen'))
    from model import identifier
    identifier(name)
    services = ROOT / 'services'
    destination = services / name
    schema_destination = ROOT / 'schemas/services' / name
    if schema_destination.exists() or schema_destination.is_symlink():
        raise ValueError(f'Контракт сервиса уже существует: {schema_destination}')
    if destination.exists() or destination.is_symlink():
        raise ValueError(f'Сервис уже существует: {destination}')
    registered = [ROOT / 'CMakeLists.txt', ROOT / '.github/service-deps.yaml', ROOT / 'docker-compose.yml']
    registry_lock = services / 'configs/docs/api/codegen.lock'
    if registry_lock.is_file():
        registered.append(registry_lock)
    before = {p: p.read_bytes() for p in registered}
    auto_schema = ROOT / 'schemas/configs' / name / (name.upper() + '_HTTP_CLIENT_CONFIG.yaml')
    auto_before = auto_schema.read_bytes() if auto_schema.exists() else None
    with tempfile.TemporaryDirectory(prefix='.new-service-', dir=services) as temporary:
        stage = Path(temporary) / name
        shutil.copytree(ROOT / 'scripts/templates/service', stage,
                        ignore=shutil.ignore_patterns('__pycache__', '*.pyc', '.DS_Store'))
        for p in stage.rglob('*'):
            if p.is_file():
                text = p.read_text()
                shared = ['broker_common', 'broker_openapi_generate', 'broker_openapi_apply']
                for index, token in enumerate(shared):
                    text = text.replace(token, f'@PLATFORM_{index}@')
                text = text.replace('broker', name)
                for index, token in enumerate(shared):
                    text = text.replace(f'@PLATFORM_{index}@', token)
                p.write_text(text)
        (stage / 'service.yaml').write_text(yaml.safe_dump({
            'version': 1, 'libraries': ['common'], 'postgresql': {'enabled': postgres}, 'grpc': {'enabled': grpc},
        }, sort_keys=False))
        (stage / '.service-features.json').unlink(missing_ok=True)
        if not postgres:
            for relative in ['postgresql', 'src/views/hello_postgres']:
                shutil.rmtree(stage / relative)
            for relative in ['docs/api/hello_postgres.yaml', 'tests/test_postgres.py']:
                (stage / relative).unlink()
        if not grpc:
            for relative in ['proto', 'src/transport/grpc']:
                shutil.rmtree(stage / relative)
            (stage / 'tests/test_grpc.py').unlink()
        compose = yaml.safe_load(before[ROOT / 'docker-compose.yml'])
        port = max([15432] + [int(str(p).split(':')[-2]) for svc in compose['services'].values() for p in svc.get('ports', [])]) + 1
        if postgres:
            for p in (stage / 'configs').glob('config_vars*.yaml'):
                p.write_text(p.read_text().replace(':15433/', f':{port}/'))
        subprocess.run([sys.executable, str(ROOT / 'scripts/openapi_gen/generate.py'), '--service-dir', str(stage)], check=True)
        subprocess.run([sys.executable, str(ROOT / 'scripts/openapi_gen/generate.py'), '--service-dir', str(stage), '--check'], check=True)
        try:
            stage.rename(destination)
            from export_clients import export
            export(destination)
            subprocess.run([sys.executable, str(ROOT / 'scripts/openapi_gen/generate.py'), '--service-dir', str(destination)], check=True)
            deps = yaml.safe_load(before[ROOT / '.github/service-deps.yaml'])
            deps['libs']['common']['services'] = sorted(set(deps['libs']['common']['services'] + [name]))
            (ROOT / '.github/service-deps.yaml').write_text(yaml.safe_dump(deps, sort_keys=False, indent=2))
            if registry_lock.exists():
                from generate import digest, load
                registry_lock.write_text(digest(registry_lock.parents[2], load(registry_lock.parents[2])))
            if postgres:
                compose['services'][f'postgres-{name}'] = {
                    'image': 'postgres:16.8-alpine',
                    'environment': {'POSTGRES_USER': 'testsuite', 'POSTGRES_PASSWORD': 'testsuite', 'POSTGRES_DB': f'{name}_db_1'},
                    'ports': [f'127.0.0.1:{port}:5432'],
                    'volumes': [f'./services/{name}/postgresql/schemas/db_1:/docker-entrypoint-initdb.d:ro'],
                    'healthcheck': {'test': ['CMD-SHELL', f'pg_isready -U testsuite -d {name}_db_1'], 'interval': '2s', 'timeout': '5s', 'retries': 10},
                }
                (ROOT / 'docker-compose.yml').write_text(yaml.safe_dump(compose, sort_keys=False))
        except BaseException:
            if auto_before is None:
                auto_schema.unlink(missing_ok=True)
                if auto_schema.parent.exists() and not any(auto_schema.parent.iterdir()):
                    auto_schema.parent.rmdir()
            else:
                auto_schema.write_bytes(auto_before)
            for p, data in before.items():
                p.write_bytes(data)
            if schema_destination.exists():
                shutil.rmtree(schema_destination)
            if destination.exists():
                shutil.rmtree(destination)
            raise
    return destination


def create(name, postgres=False, grpc=False):
    if not re.fullmatch(r'[a-z][a-z0-9_]*', name):
        raise ValueError('Некорректное имя сервиса')
    sys.path.insert(0, str(ROOT / 'scripts/openapi_gen'))
    from model import identifier
    identifier(name)
    lock = ROOT / '.service-create.lock'
    try:
        handle = lock.open('x')
    except FileExistsError:
        raise ValueError('Уже выполняется создание сервиса; повторите после завершения') from None
    try:
        with handle:
            return _create(name, postgres, grpc)
    finally:
        lock.unlink(missing_ok=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('name')
    parser.add_argument('--postgresql', action='store_true')
    parser.add_argument('--grpc', action='store_true')
    args = parser.parse_args()
    try:
        print(create(args.name, args.postgresql, args.grpc))
    except (ValueError, OSError, subprocess.CalledProcessError) as exc:
        parser.exit(1, f'{exc}\n')
