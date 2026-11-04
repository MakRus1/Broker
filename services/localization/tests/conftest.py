# SERVICE_TESTS_BEGIN
import pytest
from testsuite.databases.pgsql import discover
@pytest.fixture(scope='session')
def initial_data_path(service_source_dir):
    return [service_source_dir / 'postgresql/data']
@pytest.fixture(scope='session')
def pgsql_local(service_source_dir, pgsql_local_create):
    databases = discover.find_schemas('localization', [service_source_dir / 'postgresql/schemas'])
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
        component['platform-url'] = mockserver_info.url('/platform/access')
        config_vars['client-organizations-base-url'] = mockserver_info.base_url

    return patch


@pytest.fixture(autouse=True)
def auth_jwks(mockserver, auth_key):
    @mockserver.json_handler('/auth/jwks')
    def jwks(request):
        return {'keys': [auth_key.jwk]}


@pytest.fixture(autouse=True)
def organization_access(mockserver):
    import base64
    import json

    @mockserver.handler('/api/organizations/membership/get')
    def access(request):
        payload = json.loads(
            base64.urlsafe_b64decode(request.headers['Authorization'].split('.')[1] + '==')
        )
        allowed = {
            'author': ORGANIZATION,
            'colleague': ORGANIZATION,
            'other': OTHER_ORGANIZATION,
        }
        organization = allowed.get(payload['sub'])
        if not organization or request.json['organization_id'] != organization:
            return mockserver.make_response(
                status=403, json={'error': 'forbidden', 'message': 'Нет доступа'}
            )
        return mockserver.make_response(
            json={'organization_id': organization}
        )


@pytest.fixture
def api(service_client, auth_key):
    class Client:
        async def post(self, path, body, *, user='author', roles=None, expected=200):
            roles = roles if roles is not None else []
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


@pytest.fixture(autouse=True)
def platform_access(mockserver):
    import base64
    import json

    @mockserver.handler('/platform/access')
    def access(request):
        payload = json.loads(base64.urlsafe_b64decode(request.headers['Authorization'].split('.')[1] + '=='))
        permissions = {'translator': ['localization.read', 'localization.write'], 'reader': ['localization.read']}.get(payload['sub'], [])
        if request.json['permission'] not in permissions:
            return mockserver.make_response(status=403, json={'error': 'forbidden', 'message': 'Нет права'})
        return mockserver.make_response(json={'permissions': permissions, 'revision': 1})
