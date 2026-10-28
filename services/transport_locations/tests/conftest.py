# SERVICE_TESTS_BEGIN
import pytest
from testsuite.databases.pgsql import discover
@pytest.fixture(scope='session')
def initial_data_path(service_source_dir):
    return [service_source_dir / 'postgresql/data']
@pytest.fixture(scope='session')
def pgsql_local(service_source_dir, pgsql_local_create):
    databases = discover.find_schemas('transport_locations', [service_source_dir / 'postgresql/schemas'])
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
USERVER_CONFIG_HOOKS = ['configure_auth', 'configure_airport_sync', 'configure_sea_port_sync', 'configure_railway_sync', 'configure_sync_policies']


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
        component['organization-url'] = mockserver_info.url('/organizations/access')
        config_vars['client-organizations-base-url'] = mockserver_info.base_url
        config_vars['client-counterparties-base-url'] = mockserver_info.base_url
        config_vars['client-transportations-base-url'] = mockserver_info.base_url
        config_vars['client-transport_locations-base-url'] = mockserver_info.base_url

    return patch


@pytest.fixture(autouse=True)
def auth_jwks(mockserver, auth_key):
    @mockserver.json_handler('/auth/jwks')
    def jwks(request):
        return {'keys': [auth_key.jwk]}


@pytest.fixture(autouse=True)
def organization_access(mockserver):
    from organization_access import install

    install(mockserver, {'author': ORGANIZATION, 'other': OTHER_ORGANIZATION})


@pytest.fixture
def api(service_client, auth_key):
    class Client:
        async def post(self, path, body, *, user='author', roles=None, expected=200):
            roles = (
                roles
                if roles is not None
                else [
                    'counterparties.read',
                    'organizations.read_employees',
                    'shipments.read',
                    'transportations.read',
                    'shipments.write',
                    'transportations.create',
                ]
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
            assert response.status in (
                expected if isinstance(expected, tuple) else (expected,)
            ), response.text
            return response.json()

    return Client()


@pytest.fixture(scope='session')
def configure_airport_sync(mockserver_info):
    def patch(config, config_vars):
        component = config['components_manager']['components']['airport-sync']
        component['load-enabled'] = True
        component['url'] = mockserver_info.url('/airports.csv')
        component['minimum-records'] = 1
    return patch


@pytest.fixture(scope='session')
def configure_sea_port_sync(mockserver_info):
    def patch(config, config_vars):
        component = config['components_manager']['components']['sea-port-sync']
        component['load-enabled'] = True
        component['releases-url'] = mockserver_info.url('/unlocode/releases')
        component['download-base-url'] = mockserver_info.url('/unlocode/artifacts/')
        component['minimum-records'] = 1
    return patch


@pytest.fixture(scope='session')
def configure_railway_sync(mockserver_info):
    def patch(config, config_vars):
        component = config['components_manager']['components']['railway-sync']
        component['load-enabled'] = True
        component['url'] = mockserver_info.url('/railway')
        component['minimum-records'] = 1
    return patch


@pytest.fixture(scope='session')
def configure_sync_policies():
    def patch(config, config_vars):
        import yaml
        config_vars['config-updates-enabled'] = True
        # Проверяем реальные тайм-ауты из динамического конфига, без замены на 10s.
        config['components_manager']['components']['http-client-core'].pop('testsuite-timeout', None)
        policies = config['components_manager']['components']['dynamic-config']['defaults']
        for name in ('AIRPORT_SYNC_SETTINGS', 'SEA_PORT_SYNC_SETTINGS', 'RAILWAY_SYNC_SETTINGS'):
            path = Path(__file__).resolve().parents[3] / 'schemas/configs/transport_locations' / (name + '.yaml')
            policy = yaml.safe_load(path.read_text())['default']
            policy['http_timeout_ms'] = 1000
            policies[name] = policy
    return patch


@pytest.fixture(scope='session')
def userver_config_http_client(userver_config_http_client):
    def patch(config, config_vars):
        userver_config_http_client(config, config_vars)
        config['components_manager']['components']['http-client-core'].pop('testsuite-timeout', None)
    return patch
