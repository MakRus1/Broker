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

ROOT = Path(__file__).resolve().parent.parent


def _create(name, postgres=False, grpc=False):
    if not re.fullmatch(r'[a-z][a-z0-9_]*', name) or name in {'class', 'template', 'namespace', 'new', 'delete', 'default', 'return', 'operator'}:
        raise ValueError('Имя сервиса: строчные латинские буквы, цифры и _, начиная с буквы; ключевые слова запрещены')
    sys.path.insert(0, str(ROOT / 'scripts/openapi_gen'))
    from model import identifier
    identifier(name)
    services = ROOT / 'services'
    destination = services / name
    if destination.exists() or destination.is_symlink():
        raise ValueError(f'Сервис уже существует: {destination}')
    registered = [ROOT / 'CMakeLists.txt', ROOT / '.github/service-deps.yaml', ROOT / 'docker-compose.yml']
    before = {p: p.read_bytes() for p in registered}
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
        config_path = stage / 'configs/static_config.yaml'
        config = yaml.safe_load(config_path.read_text())
        components = config['components_manager']['components']
        main = stage / 'src/main.cpp'
        text = main.read_text()
        cmake = (stage / 'CMakeLists.txt').read_text()
        if not grpc:
            for p in [stage / 'src/hello_grpc.cpp', stage / 'src/hello_grpc.hpp', stage / 'tests/test_grpc.py']:
                p.unlink()
            shutil.rmtree(stage / 'proto')
            text = text.replace('#include <hello_grpc.hpp>\n', '').replace(f'.Append<{name}::HelloGrpc>()', '').replace('.AppendComponentList(userver::ugrpc::server::MinimalComponentList())', '')
            text = text.replace('#include <platform/grpc_telemetry.hpp>\n', '').replace('.Append<platform::GrpcTelemetryFactory>()', '')
            components.pop('platform-grpc-telemetry')
            components.pop('grpc-server-middlewares-pipeline')
            components.pop('handler-hello-grpc')
            components.pop('grpc-server')
            components.pop('grpc-server-logging')
            cmake = cmake.replace('    src/hello_grpc.cpp\n', '')
            cmake = re.sub(r'userver_add_grpc_library\([^\n]+\)\ntarget_link_libraries\([^\n]+\)\n', '', cmake)
        if not postgres:
            (stage / 'src/deps.hpp').write_text(f'#pragma once\n\n#include <userver/components/component_context.hpp>\n\nnamespace {name} {{\nstruct Deps {{}};\ninline Deps ResolveDeps(const userver::components::ComponentContext&) {{ return {{}}; }}\n}}  // namespace {name}\n')
            shutil.rmtree(stage / 'postgresql')
            shutil.rmtree(stage / 'src/views/hello_postgres')
            (stage / 'docs/api/hello_postgres.yaml').unlink()
            (stage / 'tests/test_postgres.py').unlink()
            components.pop('postgres-db-1')
            cmake = re.sub(r'# SQL_QUERIES_BEGIN.*?# SQL_QUERIES_END\n', '', cmake, flags=re.S)
            cmake = cmake.replace('userver::postgresql ', '')
            text = text.replace('#include <userver/storages/postgres/component.hpp>\n', '')
            text = text.replace('.Append<userver::components::Postgres>("postgres-db-1")', '')
            ping = stage / 'src/ping.cpp'
            s = re.sub(r',\s*pg_\(context\.FindComponent<userver::components::Postgres>\("postgres-db-1"\)\.GetCluster\(\)\)', '', ping.read_text())
            s = re.sub(r'#include <userver/storages/postgres/[^>]+>\n', '', s)
            s = s.replace('        pg_->Execute(userver::storages::postgres::ClusterHostType::kMaster, "SELECT 1");', '')
            ping.write_text(s)
            p = stage / 'src/ping.hpp'
            p.write_text(re.sub(r'#include <userver/storages/postgres/[^>]+>\n', '',
                                p.read_text().replace('    userver::storages::postgres::ClusterPtr pg_;\n', '')))
        plugins = ["pytest_userver.plugins.core"]
        if postgres:
            plugins.append('pytest_userver.plugins.postgresql')
        if grpc:
            plugins.append('pytest_userver.plugins.grpc')
        conftest = 'import pytest\npytest_plugins = ' + repr(plugins) + '\n'
        if postgres:
            conftest += f'''from testsuite.databases.pgsql import discover
@pytest.fixture(scope='session')
def initial_data_path(service_source_dir):
    return [service_source_dir / 'postgresql/data']
@pytest.fixture(scope='session')
def pgsql_local(service_source_dir, pgsql_local_create):
    databases = discover.find_schemas('{name}', [service_source_dir / 'postgresql/schemas'])
    return pgsql_local_create(list(databases.values()))
'''
        if grpc:
            conftest += '''import handlers.hello_pb2_grpc as hello_services
@pytest.fixture
def grpc_service(grpc_channel, service_client):
    return hello_services.HelloServiceStub(grpc_channel)
'''
        conftest += '''
@pytest.fixture(scope='session')
async def service_http_ping_url(monitor_baseurl):
    # Testsuite ожидает готовность всех компонентов, а не только живой процесс.
    return monitor_baseurl.rstrip('/') + '/ping/ready'
'''
        (stage / 'tests/conftest.py').write_text(conftest)
        main.write_text(text)
        from render import format_cpp
        for source in stage.rglob('*'):
            if source.suffix in {'.cpp', '.hpp'}:
                source.write_text(format_cpp(source.read_text()))
        (stage / 'CMakeLists.txt').write_text(cmake)
        # Блок ручек восстанавливает генератор; остальные настройки сериализуются безопасно.
        for key in list(components):
            if key.startswith('handler-foo-') or key.startswith('handler-hello-') or key.startswith('handler-hello_postgres-'):
                if key != 'handler-hello-grpc':
                    del components[key]
        config_text = yaml.safe_dump(config, sort_keys=False)
        config_text += '\n'  # Маркеры должны находиться внутри components.
        marker = '    # OPENAPI_HANDLERS_BEGIN\n    # OPENAPI_HANDLERS_END\n'
        # YAML отступ components — четыре пробела, генератор использует восемь: допустимо только единообразно.
        config_text = yaml.safe_dump(config, sort_keys=False, indent=4)
        anchor = '    components:\n'
        config_text = config_text.replace(anchor, anchor + '        # OPENAPI_HANDLERS_BEGIN\n        # OPENAPI_HANDLERS_END\n', 1)
        config_path.write_text(config_text)
        compose = yaml.safe_load(before[ROOT / 'docker-compose.yml'])
        port = max([15432] + [int(str(p).split(':')[-2]) for svc in compose['services'].values() for p in svc.get('ports', [])]) + 1
        if postgres:
            for p in (stage / 'configs').glob('config_vars*.yaml'):
                p.write_text(p.read_text().replace(':15433/', f':{port}/'))
        subprocess.run([sys.executable, str(ROOT / 'scripts/openapi_gen/generate.py'), '--service-dir', str(stage)], check=True)
        subprocess.run([sys.executable, str(ROOT / 'scripts/openapi_gen/generate.py'), '--service-dir', str(stage), '--check'], check=True)
        try:
            stage.rename(destination)
            deps = yaml.safe_load(before[ROOT / '.github/service-deps.yaml'])
            deps['libs']['common']['services'] = sorted(set(deps['libs']['common']['services'] + [name]))
            (ROOT / '.github/service-deps.yaml').write_text(yaml.safe_dump(deps, sort_keys=False, indent=2))
            (ROOT / 'CMakeLists.txt').write_bytes(before[ROOT / 'CMakeLists.txt'] + f'\nadd_subdirectory(services/{name})\n'.encode())
            if postgres:
                compose['services'][f'postgres-{name}'] = {
                    'image': 'postgres:16.8-alpine',
                    'environment': {'POSTGRES_USER': 'testsuite', 'POSTGRES_PASSWORD': 'testsuite', 'POSTGRES_DB': f'{name}_db_1'},
                    'ports': [f'127.0.0.1:{port}:5432'],
                    'volumes': [f'./services/{name}/postgresql/schemas/db_1.sql:/docker-entrypoint-initdb.d/01_schema.sql:ro'],
                    'healthcheck': {'test': ['CMD-SHELL', f'pg_isready -U testsuite -d {name}_db_1'], 'interval': '2s', 'timeout': '5s', 'retries': 10},
                }
                (ROOT / 'docker-compose.yml').write_text(yaml.safe_dump(compose, sort_keys=False))
        except BaseException:
            for p, data in before.items():
                p.write_bytes(data)
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
