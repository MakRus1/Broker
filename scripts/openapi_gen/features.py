"""Декларативное подключение инфраструктуры без перезаписи ручного кода."""
from pathlib import Path
from auth import policy as auth_policy
from service_accounts import policy as service_accounts_policy, load as load_service_accounts
import json
import re
import yaml
from model import identifier
from render import format_cpp
from clients import client_specs
from configs import selected, readiness_rules
from dashboards import render as render_dashboard
from http_logging import policy as logging_policy
from deployment import validate as validate_deployment
from permissions import catalog as permission_catalog

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
    if not isinstance(spec, dict) or set(spec) - {'version', 'postgresql', 'grpc', 'libraries', 'clients', 'configs', 'critical_dependencies', 'dashboard', 'http_logging', 'deployment', 'auth', 'service_accounts'} or type(spec.get('version')) is not int or spec['version'] != 1:
        raise ValueError('service.yaml: ожидается version: 1 и секции postgresql/grpc/libraries/clients/configs/critical_dependencies/dashboard/http_logging/deployment/auth/service_accounts')
    service_accounts_policy(spec)
    if service.parent.name == 'services':
        load_service_accounts(service.parent.parent, loader)
    auth = auth_policy(spec)
    logging_policy(spec)
    validate_deployment(spec.get('deployment', {}))
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
        allowed = {'enabled', 'cluster'} if key == 'postgresql' else {'enabled'}
        if not isinstance(section, dict) or set(section) - allowed or type(section.get('enabled')) is not bool:
            raise ValueError(f'service.yaml: {key} требует enabled: true/false; cluster допустим только для postgresql')
        enabled[key] = section['enabled']
    cluster = spec.get('postgresql', {}).get('cluster', 'main')
    if not isinstance(cluster, str) or not re.fullmatch(r'[a-z][a-z0-9]*(?:[-_][a-z0-9]+)*', cluster):
        raise ValueError('service.yaml: postgresql.cluster — имя из строчных букв, цифр, разделителей - или _')
    pg_component = 'postgres-' + cluster
    state_path = service / '.service-features.json'
    previous = {}
    if state_path.exists():
        previous = json.loads(state_path.read_text())
        if not isinstance(previous, dict) or set(previous) != {'postgresql', 'grpc'} or any(type(value) is not bool for value in previous.values()):
            raise ValueError('Поврежден .service-features.json: ожидаются признаки postgresql/grpc')
        for key, value in previous.items():
            if value and not enabled[key]:
                raise ValueError(f'Отключение {key} требует ручного удаления использующего его кода; автоматическое удаление запрещено')
    name = "broker" if service.resolve() == (ROOT / "scripts/templates/service").resolve() else service.name
    identifier(name)
    clients = client_specs(service, loader)
    _, _, config_entries = selected(service, loader)
    config_enabled = bool(config_entries)
    policies = readiness_rules(service, spec, loader)
    if spec.get('configs') and 'configs' not in libraries:
        raise ValueError('service.yaml: explicit configs require libraries: [common, configs]')
    if config_enabled and 'configs' not in libraries:
        libraries = [*libraries, 'configs']
    if auth and 'auth' not in libraries:
        libraries = [*libraries, 'auth']
    auth_includes = '#include <platform/auth/component.hpp>\n#include <userver/clients/http/component.hpp>\n' if auth else ''
    auth_registration = '    component_list.Append<platform::auth::Component>();\n    component_list.Append<userver::components::HttpClient>("auth-http-client");\n' if auth else ''
    config_includes = '#include <platform/configs/components.hpp>\n#include <configs/keys.hpp>\n#include <platform/configs/runtime.hpp>\n#include <userver/dynamic_config/storage/component.hpp>\n#include <userver/dynamic_config/updater/component_list.hpp>\n' if config_enabled else ''
    client_includes = ''.join(f'#include <clients/{alias}/client.hpp>\n' for alias, _ in clients)
    pg, grpc = enabled['postgresql'], enabled['grpc']
    outbox = 'outbox' in libraries
    if outbox and not pg:
        raise ValueError('service.yaml: библиотека outbox требует postgresql.enabled: true')
    outbox_include = '#include <platform/outbox/component.hpp>\n' if outbox else ''
    outbox_registration = '    component_list.Append<platform::outbox::Component>();\n' if outbox else ''
    files = {service / 'monitoring/grafana.json': render_dashboard(name, spec)}
    def edit(relative, blocks, prefix='//'):
        path = service / relative
        text = path.read_text()
        for tag, body in blocks.items():
            text = replace_block(text, tag, body, prefix)
        files[path] = format_cpp(text) if path.suffix in {'.cpp', '.hpp'} else text
    main_path = 'src/main.cpp' if (service / 'src/main.cpp').exists() else 'main.cpp'
    edit(main_path, {
        'INCLUDES': '#include <platform/readiness.hpp>\n' + auth_includes + config_includes + client_includes + outbox_include + ('#include <userver/storages/postgres/component.hpp>\n' if pg else '') + ('#include <platform/grpc_telemetry.hpp>\n#include <platform/grpc_readiness.hpp>\n' if grpc else ''),
        'REGISTRATION': '    component_list.Append<platform::Readiness>().Append<platform::HttpReadinessFactory>();\n' + auth_registration + ('    component_list.AppendComponentList(userver::dynamic_config::updater::ComponentList());\n    component_list.Append<platform::configs::DeliverySink<::configs::DeliveryContract>>();\n    component_list.Append<platform::configs::ConfigReadiness>();\n' if config_enabled else '') + ''.join(f'    component_list.Append<{name}::clients::{alias}::Component>();\n' for alias, _ in clients) + (f'    component_list.Append<userver::components::Postgres>("{pg_component}");\n' if pg else '') + outbox_registration + (f'    component_list.AppendComponentList(userver::ugrpc::server::MinimalComponentList()).Append<platform::GrpcTelemetryFactory>().Append<platform::GrpcReadinessFactory>();\n' if grpc else ''),
    })
    edit('src/deps.hpp', {
        'DEPS_INCLUDES': '#include <platform/readiness.hpp>\n' + config_includes + client_includes + ('#include <userver/storages/postgres/cluster.hpp>\n#include <userver/storages/postgres/component.hpp>\n' if pg else ''),
        'DEPS_FIELDS': '    platform::Readiness* readiness;\n' + ('    platform::configs::Source configs;\n' if config_enabled else '') + ''.join(f'    clients::{alias}::Client {alias};\n' for alias, _ in clients) + ('    userver::storages::postgres::ClusterPtr pg_cluster;\n' if pg else ''),
        'DEPS_INIT': '        .readiness = &component_context.FindComponent<platform::Readiness>(),\n' + ('        .configs = platform::configs::Source{component_context.FindComponent<userver::components::DynamicConfig>().GetSource()},\n' if config_enabled else '') + ''.join(f'        .{alias} = component_context.FindComponent<clients::{alias}::Component>().GetClient(),\n' for alias, _ in clients) + (f'        .pg_cluster = component_context.FindComponent<userver::components::Postgres>("{pg_component}").GetCluster(),\n' if pg else ''),
    })
    if (service / 'src/bootstrap/ping.cpp').exists():
        edit('src/bootstrap/ping.cpp', {
            'PING_INCLUDES': '#include <userver/storages/postgres/cluster.hpp>\n' if pg else '',
            'PING_CHECK': '        const auto readiness = deps_.readiness->GetStatus();\n        if (!readiness.Ready) { request.SetResponseStatus(userver::server::http::HttpStatus::kServiceUnavailable); return readiness.Reason + "\\n"; }\n' + ('        deps_.pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster, "SELECT 1");\n' if pg else ''),
        })
    cmake = 'foreach(service_library IN ITEMS ' + ' '.join('broker_' + lib for lib in libraries) + ')\n'
    cmake += '    if(NOT TARGET ${service_library})\n        message(FATAL_ERROR "Не найден target библиотеки: ${service_library}")\n    endif()\nendforeach()\n'
    cmake += 'target_link_libraries(${PROJECT_NAME}_objs PUBLIC ' + ' '.join('broker_' + lib for lib in libraries) + ')\n'
    if pg:
        cmake += '''target_link_libraries(${PROJECT_NAME}_objs PUBLIC userver::postgresql)
file(GLOB_RECURSE service_sql CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/postgresql/queries/*.sql")
if(service_sql)
userver_add_sql_library(${PROJECT_NAME}_sql
    NAMESPACE ${PROJECT_NAME}
    SOURCE_DIR postgresql/queries
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated_sql
    SQL_FILES *.sql)
target_link_libraries(${PROJECT_NAME}_objs PUBLIC ${PROJECT_NAME}_sql)
endif()
'''
    if grpc:
        cmake += '''target_link_libraries(${PROJECT_NAME}_objs PUBLIC userver::grpc)
file(GLOB_RECURSE service_protos CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto")
if(service_protos)
userver_add_grpc_library(${PROJECT_NAME}_proto PROTOS ${service_protos})
target_link_libraries(${PROJECT_NAME}_objs PUBLIC ${PROJECT_NAME}_proto)
endif()
'''
    edit('CMakeLists.txt', {'CMAKE': cmake}, '#')
    if outbox:
        directory = service / 'postgresql/schemas/db_1'
        if not list(directory.glob('V*__create_outbox.sql')):
            versions = [int(path.name[1:5]) for path in directory.glob('V[0-9][0-9][0-9][0-9]__*.sql')]
            version = max(versions, default=0) + 1
            files[directory / f'V{version:04d}__create_outbox.sql'] = (ROOT / 'libs/outbox/postgresql/schema.sql').read_text()
    if pg:
        for directory in ('postgresql/schemas/db_1', 'postgresql/queries'):
            if not any((service / directory).glob('*.sql')):
                files[service / directory / '.gitkeep'] = ''
    if grpc and not any((service / 'proto').rglob('*.proto')):
        files[service / 'proto/.gitkeep'] = ''
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
    tests += f'pytest_plugins = {plugins!r}\n'
    edit('tests/conftest.py', {'TESTS': tests}, '#')
    components = {f'client-{alias}': {'base-url': f'$client-{alias}-base-url'} for alias, _ in clients}
    components['platform-readiness'] = {}
    components['platform-http-readiness'] = {}
    if config_enabled:
        components['platform-config-delivery'] = {'load-enabled': '$config-updates-enabled'}
        components['platform-config-readiness'] = {'rules': policies}
    if outbox:
        components['platform-outbox'] = {'postgres-component': pg_component}
    if pg:
        components[pg_component] = {'dbconnection': '$pg-connection', 'dbconnection#env': 'DB_CONNECTION', 'blocking_task_processor': 'fs-task-processor', 'dns_resolver': 'async', 'sync-start': True, 'connlimit_mode': 'manual'}
    if grpc:
        components.update({'platform-grpc-telemetry': {}, 'platform-grpc-readiness': {}, 'grpc-server-middlewares-pipeline': {'middlewares': {'platform-grpc-telemetry': {'enabled': True}, 'platform-grpc-readiness': {'enabled': True}}}, 'grpc-server-logging': {'msg-log-level': 'none'}, 'grpc-server': {'port': '$grpc-server-port', 'completion-queue-count': 3}})
    path = service / 'configs/static_config.yaml'
    static_source = path.read_text()
    managed = static_source.split('# SERVICE_CONFIG_BEGIN', 1)[1].split('# SERVICE_CONFIG_END', 1)[0]
    if not auth and re.search(r'(?m)^        platform-auth:', managed):
        raise ValueError('service.yaml: удаление auth требует явного удаления его сгенерированной обвязки и защищенных ручек')
    if auth:
        existing_components = yaml.load(static_source, Loader=loader)['components_manager']['components']
        pipeline = dict(existing_components.get('platform-http-pipeline', {}))
        extras = pipeline.get('extra-middlewares', [])
        if not isinstance(extras, list) or any(not isinstance(item, str) for item in extras):
            raise ValueError('platform-http-pipeline.extra-middlewares должен быть списком')
        pipeline['extra-middlewares'] = [item for item in extras if item != 'platform-auth'] + ['platform-auth']
        components['platform-http-pipeline'] = pipeline
        components['platform-auth'] = {
            'issuer': '$auth-issuer', 'audience': auth['audience'],
            'roles-client': auth['roles-client'], 'jwks-url': '$auth-jwks-url',
            'protected-prefix': auth['protected-prefix'], 'allow-http': '$auth-allow-http',
            'permissions': dict(sorted(auth['permissions'].items())),
        }
        rights = permission_catalog(ROOT, loader)
        dependencies = {code: value['requires'] for code, value in rights.items() if value['requires']}
        if dependencies:
            components['platform-auth']['permission-dependencies'] = dependencies
        if auth.get('service-clients'):
            components['platform-auth']['service-clients'] = auth['service-clients']
        if auth.get('platform-access'):
            components['platform-auth']['platform-url'] = '$platform-access-url'
        if auth.get('organization-access'):
            components['platform-auth']['organization-url'] = '$organization-access-url'
        components['auth-http-client'] = {'wait-for-dynamic-configs': False}
        # Эти компоненты принадлежат генератору; остальная ручная конфигурация сохраняется.
        for component in ('platform-auth', 'auth-http-client', 'platform-http-pipeline'):
            static_source = re.sub(r'(?m)^        ' + component + r':[^\n]*\n(?: {9,}.*\n| *\n)*', '', static_source)
        if existing_components['server'].get('middleware-pipeline-builder') != 'platform-http-pipeline':
            raise ValueError('service.yaml: auth требует server.middleware-pipeline-builder: platform-http-pipeline')
    if config_enabled:
        existing = yaml.load(static_source, Loader=loader)['components_manager']['components'].get('dynamic-config', {}) or {}
        components['dynamic-config'] = {**existing, 'updates-enabled': '$config-updates-enabled', 'fs-cache-path': '$config-cache-path', 'fs-task-processor': 'fs-task-processor'}
        components['dynamic-config-client'] = {'load-enabled': '$config-updates-enabled', 'config-url': '$config-server-url', 'http-client': 'dynamic-config-http-client', 'service-name': name, 'get-configs-overrides-for-service': False, 'http-timeout': '1s', 'http-retries': 1}
        components['dynamic-config-client-updater'] = {'load-enabled': '$config-updates-enabled', 'config-settings': False, 'first-update-fail-ok': True, 'updates-sink': 'platform-config-delivery', 'update-types': 'only-full', 'deduplicate-update-types': 'none', 'update-interval': '1s'}
        static_source = re.sub(r'(?m)^        dynamic-config:\n(?: {12}.*\n| *\n)*', '', static_source)
    body = yaml.safe_dump(components, sort_keys=False, indent=4) if components else ''
    text = replace_block(static_source, 'CONFIG', body, '#')
    # Блок компонентов всегда имеет тот же отступ, что и HTTP-ручки.
    start = text.index('# SERVICE_CONFIG_BEGIN');end = text.index('# SERVICE_CONFIG_END', start) + len('# SERVICE_CONFIG_END')
    text = text[:start] + '\n'.join('        ' + line if line else '' for line in text[start:end].splitlines()) + text[end:]
    files[path] = text
    for path in (service / 'configs').glob('config_vars*.yaml'):
        values = yaml.load(path.read_text(), Loader=loader)
        additions = {}
        if auth:
            for key, value in {
                'auth-issuer': 'http://localhost:18888/realms/broker',
                'auth-jwks-url': 'http://localhost:18888/realms/broker/protocol/openid-connect/certs',
                'auth-allow-http': True,
            }.items():
                if key not in values:
                    additions[key] = value
        if config_enabled:
            for key, value in {'config-updates-enabled': False, 'config-server-url': 'http://localhost:8090', 'config-cache-path': ''}.items():
                if key not in values:
                    additions[key] = value
        for alias, _ in clients:
            for suffix, value in [('base-url', 'http://localhost:8080')]:
                key = f'client-{alias}-{suffix}'
                if key not in values:
                    additions[key] = value
        if pg and 'pg-connection' not in values:
            additions['pg-connection'] = f'postgresql://testsuite:testsuite@127.0.0.1:15433/{name}_db_1'
        if grpc and 'grpc-server-port' not in values:
            additions['grpc-server-port'] = 8081
        if additions:
            files[path] = path.read_text().rstrip() + '\n' + yaml.safe_dump(additions, sort_keys=False)
    files[state_path] = json.dumps(enabled, indent=2) + '\n'
    return files
