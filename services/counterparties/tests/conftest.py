# SERVICE_TESTS_BEGIN
import pytest
from testsuite.databases.pgsql import discover
@pytest.fixture(scope='session')
def initial_data_path(service_source_dir):
    return [service_source_dir / 'postgresql/data']
@pytest.fixture(scope='session')
def pgsql_local(service_source_dir, pgsql_local_create):
    databases = discover.find_schemas('counterparties', [service_source_dir / 'postgresql/schemas'])
    return pgsql_local_create(list(databases.values()))
pytest_plugins = ['pytest_userver.plugins.core', 'pytest_userver.plugins.postgresql']
# SERVICE_TESTS_END

@pytest.fixture(scope='session')
async def service_http_ping_url(monitor_baseurl):
    # Готовность проверяется на внутреннем listener до первого запроса testsuite.
    return monitor_baseurl.rstrip('/') + '/ping/ready'

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / 'tests/support'))
from auth_tokens import ISSUER, SigningKey

ORGANIZATION = '11111111-1111-4111-8111-111111111111'
OTHER_ORGANIZATION = '22222222-2222-4222-8222-222222222222'
USERVER_CONFIG_HOOKS = ['configure_auth']


@pytest.fixture(scope='session')
def auth_key(tmp_path_factory):
    return SigningKey(tmp_path_factory.mktemp('jwt') / 'key.pem', 'logistics')


@pytest.fixture(scope='session')
def configure_auth(mockserver_info):
    def patch(config, config_vars):
        component = config['components_manager']['components']['platform-auth']
        component['issuer'] = ISSUER
        component['jwks-url'] = mockserver_info.url('/auth/jwks')
        component['allow-http'] = True

    return patch


@pytest.fixture(autouse=True)
def auth_jwks(mockserver, auth_key):
    @mockserver.json_handler('/auth/jwks')
    def jwks(request):
        return {'keys': [auth_key.jwk]}


@pytest.fixture(autouse=True)
def memberships(pgsql):
    db = pgsql['db_1']
    with db.cursor() as cursor:
        cursor.execute(
            'INSERT INTO organization_memberships VALUES (%s,%s,%s),(%s,%s,%s)',
            (ISSUER, 'author', ORGANIZATION, ISSUER, 'other', OTHER_ORGANIZATION),
        )


@pytest.fixture
def api(service_client, auth_key):
    class Client:
        async def post(self, path, body, *, user='author', roles=None, expected=200):
            roles = (
                roles
                if roles is not None
                else ['counterparties.read', 'counterparties.write']
            )
            token = auth_key.token(
                user,
                roles=roles,
                claims={
                    'aud': 'broker-logistics',
                    'resource_access': {'broker-logistics': {'roles': roles}},
                },
            )
            response = await service_client.post(
                path, json=body, headers={'Authorization': 'Bearer ' + token}
            )
            assert response.status == expected, response.text
            return response.json()

    return Client()
