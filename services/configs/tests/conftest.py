# SERVICE_TESTS_BEGIN
import pytest
from testsuite.databases.pgsql import discover
@pytest.fixture(scope='session')
def initial_data_path(service_source_dir):
    return [service_source_dir / 'postgresql/data']
@pytest.fixture(scope='session')
def pgsql_local(service_source_dir, pgsql_local_create):
    databases = discover.find_schemas('configs', [service_source_dir / 'postgresql/schemas'])
    return pgsql_local_create(list(databases.values()))
pytest_plugins = ['pytest_userver.plugins.core', 'pytest_userver.plugins.postgresql']
# SERVICE_TESTS_END

@pytest.fixture(scope='session')
async def service_http_ping_url(monitor_baseurl):
    # Готовность проверяется на внутреннем listener до первого запроса testsuite.
    return monitor_baseurl.rstrip('/') + '/ping/ready'


import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tests/support"))
from auth_tokens import ISSUER, SigningKey


@pytest.fixture(scope='session')
def auth_keys(tmp_path_factory):
    directory = tmp_path_factory.mktemp('auth-keys')
    return [SigningKey(directory / f'key-{index}.pem', f'key-{index}') for index in range(2)]


USERVER_CONFIG_HOOKS = ['configure_auth']


@pytest.fixture(scope='session')
def configure_auth(mockserver_info):
    def patch(config, config_vars):
        component = config['components_manager']['components']['platform-auth']
        component['issuer'] = ISSUER
        component['jwks-url'] = mockserver_info.url('/auth/jwks')
        component['allow-http'] = True
    return patch


@pytest.fixture(autouse=True)
def auth_jwks(mockserver, auth_keys):
    state = {'keys': [auth_keys[0].jwk], 'status': 200, 'failures': 0}

    @mockserver.json_handler('/auth/jwks')
    def jwks(request):
        if state['status'] != 200:
            state['failures'] += 1
        return mockserver.make_response(json={'keys': state['keys']}, status=state['status'])

    return state


@pytest.fixture
def access_token(auth_keys):
    return auth_keys[0].token


@pytest.fixture
def admin_client(monitor_client, access_token):
    class Client:
        async def post(self, path, *, user='author', headers=None, **kwargs):
            return await monitor_client.post(path, headers={
                'Authorization': 'Bearer ' + access_token(user), **(headers or {})}, **kwargs)

        async def get(self, path, *, user='author', headers=None, **kwargs):
            return await monitor_client.get(path, headers={
                'Authorization': 'Bearer ' + access_token(user), **(headers or {})}, **kwargs)
    return Client()
