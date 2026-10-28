# SERVICE_TESTS_BEGIN
import pytest
from testsuite.databases.pgsql import discover
@pytest.fixture(scope='session')
def initial_data_path(service_source_dir):
    return [service_source_dir / 'postgresql/data']
@pytest.fixture(scope='session')
def pgsql_local(service_source_dir, pgsql_local_create):
    databases = discover.find_schemas('shipments', [service_source_dir / 'postgresql/schemas'])
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
def configure_dispatcher(mockserver_info):
    def patch(config, config_vars):
        components = config['components_manager']['components']
        components['transportation-dispatcher']['load-enabled'] = True
        components['service-token-provider'].update(
            {
                'load-enabled': True,
                'endpoint': mockserver_info.url('/service-token'),
                'client-id': 'testsuite-dispatcher',
                'client-secret': 'testsuite-only-secret',
                'allow-http': True,
            }
        )
        components['client-transportations']['base-url'] = mockserver_info.url('/').rstrip('/')

    return patch


@pytest.fixture(autouse=True)
def shipment_dictionaries(pgsql, service_source_dir):
    # Testsuite очищает таблицы между тестами; используем тот же источник справочников.
    migration = (
        service_source_dir / 'postgresql/schemas/db_1/V0013__create_shipment_dictionaries.sql'
    )
    seed = migration.read_text().split('INSERT INTO shipment_dictionary_values', 1)[1]
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('INSERT INTO shipment_dictionary_values' + seed)


@pytest.fixture(name='userver_config_testsuite', scope='session')
def _shipment_config_updates(userver_config_testsuite):
    def patch(config, config_vars):
        userver_config_testsuite(config, config_vars)
        config_vars['config-updates-enabled'] = True

    return patch


@pytest.fixture(autouse=True)
def employee_directory(mockserver):
    @mockserver.json_handler('/api/organizations/employees/list')
    def employees(request):
        body = request.json
        subject = body.get('subject')
        items = [
            {'subject': 'logist-author', 'display_name': 'Иванов Иван'},
            {'subject': 'logist-default', 'display_name': 'Петров Пётр'},
        ]
        return {'items': [item for item in items if not subject or item['subject'] == subject]}


@pytest.fixture(autouse=True)
def customer_directory(mockserver):
    @mockserver.json_handler('/api/v1/counterparties/get')
    def customer(request):
        return {
            'counterparty': {
                'id': request.json['id'],
                'name': 'Клиент',
                'revision': 1,
                'archived': False,
                'legal_name': None,
                'country_code': None,
                'tax_id': None,
                'registration_id': None,
                'legal_address': None,
                'roles': ['customer'],
                'contacts': [],
                'logist': None,
            }
        }


@pytest.fixture(autouse=True)
def organization_administration(mockserver):
    import base64
    import json

    @mockserver.json_handler('/api/organizations/administration/check')
    def administration(request):
        token = request.headers['Authorization'].split()[1].split('.')[1]
        claims = json.loads(base64.urlsafe_b64decode(token + '=' * (-len(token) % 4)))
        expected = {'author': ORGANIZATION, 'other': OTHER_ORGANIZATION}.get(claims['sub'])
        if expected != request.json['organization_id']:
            return mockserver.make_response(
                status=403, json={'error': 'forbidden', 'message': 'Forbidden'}
            )
        return {'organization_id': expected}


@pytest.fixture(autouse=True)
def transportation_route_usage(mockserver):
    used = set()
    @mockserver.json_handler('/api/v1/transportation-routes/usage')
    def usage(request):
        return {'segment_ids': sorted(used)}
    return used


@pytest.fixture(autouse=True)
def transport_locations_lookup(mockserver, pgsql):
    @mockserver.json_handler('/api/v1/route-locations/get-batch')
    def lookup(request):
        requested = request.json['ids']
        with pgsql['db_1'].cursor() as cursor:
            cursor.execute("SELECT id, kind, name, country_code, code FROM route_locations WHERE NOT archived AND id = ANY(%s::uuid[])", (requested,))
            rows = cursor.fetchall()
        items = [dict(zip(['id', 'kind', 'name', 'country_code', 'code'], row)) for row in rows]
        for item in items:
            item['id'] = str(item['id'])
            if item['code'] is None: del item['code']
        return {'items': items}
