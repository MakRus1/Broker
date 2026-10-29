# SERVICE_TESTS_BEGIN
import pytest
from testsuite.databases.pgsql import discover
@pytest.fixture(scope='session')
def initial_data_path(service_source_dir):
    return [service_source_dir / 'postgresql/data']
@pytest.fixture(scope='session')
def pgsql_local(service_source_dir, pgsql_local_create):
    databases = discover.find_schemas('transportations', [service_source_dir / 'postgresql/schemas'])
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
        config_vars['client-shipments-base-url'] = mockserver_info.base_url
        config_vars['client-transport_locations-base-url'] = mockserver_info.base_url
        config_vars['client-counterparties-base-url'] = mockserver_info.base_url

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
                else ['transportations.read', 'transportations.write', 'transportations.create']
            )
            token = auth_key.token(
                user,
                roles=roles,
                claims={
                    'aud': 'broker-logistics',
                    'azp': 'shipments-dispatcher' if user == 'author' else 'broker-web',
                    'resource_access': {'broker-logistics': {'roles': roles}},
                },
            )
            response = await service_client.post(
                path, json=body, headers={'Authorization': 'Bearer ' + token}
            )
            assert response.status == expected, response.text
            return response.json()

    return Client()


@pytest.fixture(autouse=True)
def shipment_routes(mockserver):
    contexts = {}
    registrations = []

    @mockserver.json_handler('/api/v1/shipments/transportation-contexts')
    def load(request):
        return {'items': [contexts.get(identifier, {
            'shipment_id': identifier, 'revision': 1, 'route_segments': [],
        }) for identifier in request.json['shipment_ids']]}

    @mockserver.json_handler('/api/v1/shipments/route-segments/register-usage')
    def register(request):
        registrations.append(request.json)
        return {'registered': True}

    class Source:
        def set(self, shipment_id, route, **terms):
            contexts[shipment_id] = {
                'shipment_id': shipment_id, 'revision': 1,
                'route_segments': [{'id': item['id'], 'type': item.get('mode', item.get('type')), **{key:item[key] for key in ('sea','air') if key in item}} for item in route],
                **terms,
            }
    source = Source()
    source.registrations = registrations
    return source


@pytest.fixture(autouse=True)
def counterparties(mockserver):
    @mockserver.json_handler('/api/v1/counterparties/get')
    def get(request):
        return {'counterparty': {'id': request.json['id'], 'name': 'Перевозчик', 'revision': 1,
            'archived': False, 'own_company': False, 'legal_name': None, 'country_code': None, 'tax_id': None,
            'registration_id': None, 'legal_address': None, 'roles': ['carrier'], 'contacts': [], 'logist': None}}


@pytest.fixture(autouse=True)
def transport_locations_lookup(mockserver, pgsql):
    @mockserver.json_handler('/api/v1/railway-stations/get-batch')
    def lookup(request):
        requested = request.json['ids']
        with pgsql['db_1'].cursor() as cursor:
            cursor.execute("SELECT md5('broker:railway:esr:' || esr_code)::uuid::text, esr_code, name, country_code FROM railway_stations WHERE NOT archived AND md5('broker:railway:esr:' || esr_code)::uuid::text = ANY(%s::text[])", (requested,))
            rows = cursor.fetchall()
        items = [dict(zip(['id', 'esr_code', 'name', 'country_code'], row)) for row in rows]
        return {'items': items}
