"""Декларативное подключение инфраструктуры без перезаписи ручного кода."""
from pathlib import Path
import json
import re
import yaml
from model import identifier
from render import format_cpp

ROOT = Path(__file__).resolve().parents[2]


def replace_block(text, tag, body, prefix='//'):
    begin, end = f'{prefix} SERVICE_{tag}_BEGIN', f'{prefix} SERVICE_{tag}_END'
    if text.count(begin) != 1 or text.count(end) != 1 or text.index(begin) > text.index(end):
        raise ValueError(f'Ожидается одна пара маркеров {begin} / {end}')
    return re.sub(r'(?m)^ *' + re.escape(begin) + r'.*?^ *' + re.escape(end),
                  lambda _: begin + '\n' + body.rstrip() + '\n' + end, text, flags=re.S)


def prepare(service, loader):
    """Вернуть ожидаемые файлы; проверка и сборка ничего не записывают."""
    manifest = service / 'service.yaml'
    if not manifest.exists():
        return {}
    spec = yaml.load(manifest.read_text(), Loader=loader)
    if not isinstance(spec, dict) or set(spec) - {'version', 'postgresql', 'grpc', 'libraries'} or type(spec.get('version')) is not int or spec['version'] != 1:
        raise ValueError('service.yaml: ожидается version: 1 и секции postgresql/grpc/libraries')
    libraries = spec.get('libraries', ['common'])
    if not isinstance(libraries, list) or any(not isinstance(lib, str) or not re.fullmatch(r'[a-z][a-z0-9_]*', lib) for lib in libraries) or len(set(libraries)) != len(libraries):
        raise ValueError('service.yaml: libraries — список уникальных имен каталогов libs/<name>')
    if 'common' not in libraries:
        raise ValueError('service.yaml: библиотека common обязательна для телеметрии и инфраструктуры сервиса')
    for lib in libraries:
        if not (ROOT / 'libs' / lib / 'CMakeLists.txt').is_file():
            raise ValueError(f'service.yaml: неизвестная библиотека {lib}')
    enabled = {}
    for key in ('postgresql', 'grpc'):
        section = spec.get(key, {'enabled': False})
        if not isinstance(section, dict) or set(section) != {'enabled'} or type(section['enabled']) is not bool:
            raise ValueError(f'service.yaml: {key} должен содержать только enabled: true/false')
        enabled[key] = section['enabled']
    state_path = service / '.service-features.json'
    if state_path.exists():
        previous = json.loads(state_path.read_text())
        if not isinstance(previous, dict) or set(previous) != {'postgresql', 'grpc'} or any(type(value) is not bool for value in previous.values()):
            raise ValueError('Поврежден .service-features.json: ожидаются признаки postgresql/grpc')
        for key, value in previous.items():
            if value and not enabled[key]:
                raise ValueError(f'Отключение {key} требует ручного удаления использующего его кода; автоматическое удаление запрещено')
    name = "broker" if service.resolve() == (ROOT / "scripts/templates/service").resolve() else service.name
    identifier(name)
    pg, grpc = enabled['postgresql'], enabled['grpc']
    files = {}
    def edit(relative, blocks, prefix='//'):
        path = service / relative
        text = path.read_text()
        for tag, body in blocks.items():
            text = replace_block(text, tag, body, prefix)
        files[path] = format_cpp(text) if path.suffix in {'.cpp', '.hpp'} else text
    edit('src/main.cpp', {
        'INCLUDES': ('#include <userver/storages/postgres/component.hpp>\n' if pg else '') + ('#include <transport/grpc/hello_grpc.hpp>\n#include <platform/grpc_telemetry.hpp>\n' if grpc else ''),
        'REGISTRATION': ('    component_list.Append<userver::components::Postgres>("postgres-db-1");\n' if pg else '') + (f'    component_list.AppendComponentList(userver::ugrpc::server::MinimalComponentList()).Append<{name}::HelloGrpc>().Append<platform::GrpcTelemetryFactory>();\n' if grpc else ''),
    })
    edit('src/deps.hpp', {
        'DEPS_INCLUDES': '#include <userver/storages/postgres/cluster.hpp>\n#include <userver/storages/postgres/component.hpp>\n' if pg else '',
        'DEPS_FIELDS': '    userver::storages::postgres::ClusterPtr pg_cluster;\n' if pg else '',
        'DEPS_INIT': '        .pg_cluster = component_context.FindComponent<userver::components::Postgres>("postgres-db-1").GetCluster(),\n' if pg else '',
    })
    edit('src/bootstrap/ping.cpp', {
        'PING_INCLUDES': '#include <userver/storages/postgres/cluster.hpp>\n' if pg else '',
        'PING_CHECK': '        deps_.pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster, "SELECT 1");\n' if pg else '',
    })
    cmake = 'foreach(service_library IN ITEMS ' + ' '.join('broker_' + lib for lib in libraries) + ')\n'
    cmake += '    if(NOT TARGET ${service_library})\n        message(FATAL_ERROR "Не найден target библиотеки: ${service_library}")\n    endif()\nendforeach()\n'
    cmake += 'target_link_libraries(${PROJECT_NAME}_objs PUBLIC ' + ' '.join('broker_' + lib for lib in libraries) + ')\n'
    if pg:
        cmake += '''target_link_libraries(${PROJECT_NAME}_objs PUBLIC userver::postgresql)
file(GLOB_RECURSE service_sql CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/postgresql/queries/*.sql")
userver_add_sql_library(${PROJECT_NAME}_sql
    NAMESPACE ${PROJECT_NAME}
    SOURCE_DIR postgresql/queries
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated_sql
    SQL_FILES *.sql)
target_link_libraries(${PROJECT_NAME}_objs PUBLIC ${PROJECT_NAME}_sql)
'''
    if grpc:
        cmake += '''target_sources(${PROJECT_NAME}_objs PRIVATE src/transport/grpc/hello_grpc.cpp)
file(GLOB_RECURSE service_protos CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto")
userver_add_grpc_library(${PROJECT_NAME}_proto PROTOS ${service_protos})
target_link_libraries(${PROJECT_NAME}_objs PUBLIC userver::grpc ${PROJECT_NAME}_proto)
'''
    edit('CMakeLists.txt', {'CMAKE': cmake}, '#')
    template = ROOT / 'scripts/templates/service'
    paths = []
    if pg:
        paths += ['postgresql', 'src/views/hello_postgres', 'docs/api/hello_postgres.yaml', 'tests/test_postgres.py']
    if grpc:
        paths += ['proto', 'src/transport/grpc', 'tests/test_grpc.py']
    for relative in paths:
        source = template / relative
        for p in (source.rglob('*') if source.is_dir() else [source]):
            if p.is_file() and '__pycache__' not in p.parts and p.suffix != '.pyc':
                target = service / p.relative_to(template)
                if not target.exists():
                    files[target] = p.read_text().replace('broker', name)
    plugins = ['pytest_userver.plugins.core']
    tests = 'import pytest\n'
    if pg:
        plugins.append('pytest_userver.plugins.postgresql')
        tests += f'''from testsuite.databases.pgsql import discover
@pytest.fixture(scope='session')
def initial_data_path(service_source_dir):
    return [service_source_dir / 'postgresql/data']
@pytest.fixture(scope='session')
def pgsql_local(service_source_dir, pgsql_local_create):
    databases = discover.find_schemas('{name}', [service_source_dir / 'postgresql/schemas'])
    return pgsql_local_create(list(databases.values()))
'''
    if grpc:
        plugins.append('pytest_userver.plugins.grpc')
        tests += '''import handlers.hello_pb2_grpc as hello_services
@pytest.fixture
def grpc_service(grpc_channel, service_client):
    return hello_services.HelloServiceStub(grpc_channel)
'''
    tests += f'pytest_plugins = {plugins!r}\n'
    edit('tests/conftest.py', {'TESTS': tests}, '#')
    components = {}
    if pg:
        components['postgres-db-1'] = {'dbconnection': '$pg-connection', 'dbconnection#env': 'DB_CONNECTION', 'blocking_task_processor': 'fs-task-processor', 'dns_resolver': 'async', 'sync-start': True, 'connlimit_mode': 'manual'}
    if grpc:
        components.update({'platform-grpc-telemetry': {}, 'grpc-server-middlewares-pipeline': {'middlewares': {'platform-grpc-telemetry': {'enabled': True}}}, 'grpc-server-logging': {'msg-log-level': 'none'}, 'grpc-server': {'port': '$grpc-server-port', 'completion-queue-count': 3}, 'handler-hello-grpc': {'task-processor': 'main-task-processor'}})
    path = service / 'configs/static_config.yaml'
    body = yaml.safe_dump(components, sort_keys=False, indent=4) if components else ''
    text = replace_block(path.read_text(), 'CONFIG', body, '#')
    # Блок компонентов всегда имеет тот же отступ, что и HTTP-ручки.
    start = text.index('# SERVICE_CONFIG_BEGIN');end = text.index('# SERVICE_CONFIG_END', start) + len('# SERVICE_CONFIG_END')
    text = text[:start] + '\n'.join('        ' + line if line else '' for line in text[start:end].splitlines()) + text[end:]
    files[path] = text
    for path in (service / 'configs').glob('config_vars*.yaml'):
        values = yaml.load(path.read_text(), Loader=loader)
        additions = {}
        if pg and 'pg-connection' not in values:
            additions['pg-connection'] = f'postgresql://testsuite:testsuite@127.0.0.1:15433/{name}_db_1'
        if grpc and 'grpc-server-port' not in values:
            additions['grpc-server-port'] = 8081
        if additions:
            files[path] = path.read_text().rstrip() + '\n' + yaml.safe_dump(additions, sort_keys=False)
    files[state_path] = json.dumps(enabled, indent=2) + '\n'
    return files
