import handlers.hello_pb2_grpc as hello_services
import pytest

from testsuite.databases.pgsql import discover

pytest_plugins = [
    'pytest_userver.plugins.core',
    'pytest_userver.plugins.postgresql',
    'pytest_userver.plugins.grpc',
]


@pytest.fixture
def grpc_service(grpc_channel, service_client):
    return hello_services.HelloServiceStub(grpc_channel)


@pytest.fixture(scope='session')
def initial_data_path(service_source_dir):
    return [
        service_source_dir / 'postgresql/data',
    ]


@pytest.fixture(scope='session')
def pgsql_local(service_source_dir, pgsql_local_create):
    databases = discover.find_schemas(
        'broker',
        [service_source_dir.joinpath('postgresql/schemas')],
    )
    return pgsql_local_create(list(databases.values()))


@pytest.fixture(scope='session')
async def service_http_ping_url(monitor_baseurl):
    # Готовность проверяется на внутреннем listener до первого запроса testsuite.
    return monitor_baseurl.rstrip('/') + '/ping/ready'


USERVER_CONFIG_HOOKS = ['userver_config_trace_probe']


@pytest.fixture(scope='session')
def service_env(service_env):
    return {**service_env, 'BROKER_TRACE_TESTS': '1'}


@pytest.fixture(scope='session')
def userver_config_trace_probe():
    # Тестовые компоненты отсутствуют в обычной конфигурации сервиса.
    def patch(config, config_vars):
        components = config['components_manager']['components']
        components['grpc-probe-factory'] = {'load-enabled': '$is-testing'}
        components['handler-trace-probe'] = {
            'load-enabled': '$is-testing',
            'path': '/internal/trace-probe',
            'method': 'GET',
            'task_processor': 'main-task-processor',
        }
    return patch
