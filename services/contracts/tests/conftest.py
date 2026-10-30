# SERVICE_TESTS_BEGIN
import pytest
from testsuite.databases.pgsql import discover
@pytest.fixture(scope='session')
def initial_data_path(service_source_dir):
    return [service_source_dir / 'postgresql/data']
@pytest.fixture(scope='session')
def pgsql_local(service_source_dir, pgsql_local_create):
    databases = discover.find_schemas('contracts', [service_source_dir / 'postgresql/schemas'])
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
                else ['contracts.read', 'contracts.write', 'counterparties.read', 'counterparties.read_bank_accounts']
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
            if response.headers.get('Content-Type', '').startswith('application/json'):
                return response.json()
            return {'message': response.text}

    return Client()



OWN = '33333333-3333-4333-8333-333333333333'
PARTY = '44444444-4444-4444-8444-444444444444'
ACCOUNT = '55555555-5555-4555-8555-555555555555'

@pytest.fixture
def directory(mockserver):
    state = {'own_company': True, 'archived': False, 'status': 200, 'currency': 'RUB', 'account_archived': False}

    @mockserver.handler('/api/v1/counterparties/get')
    def get(request):
        if state['status'] != 200:
            return mockserver.make_response(status=state['status'],json={'error':'unavailable','message':'Ошибка'})
        value = request.json['id']
        return mockserver.make_response(json={'counterparty':{
            'id':value,'name':'Компания','revision':1,'archived':state['archived'],
            'own_company':state['own_company'] and value == OWN,
            'legal_name':None,'country_code':'RU','tax_id':None,'registration_id':None,
            'legal_address':None,'roles':[],'contacts':[],
        }})

    @mockserver.json_handler('/api/v1/counterparties/bank-accounts/list')
    def accounts(request):
        return {'items':[{'id':ACCOUNT,'counterparty_id':request.json['counterparty_id'],
            'name':'Счёт','number':'123','currency':state['currency'],'bank_name':'Банк',
            'is_primary':True,'archived':state['account_archived'],'revision':1}]}
    return state
