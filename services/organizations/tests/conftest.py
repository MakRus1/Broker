# SERVICE_TESTS_BEGIN
import pytest
from testsuite.databases.pgsql import discover
@pytest.fixture(scope='session')
def initial_data_path(service_source_dir):
    return [service_source_dir / 'postgresql/data']
@pytest.fixture(scope='session')
def pgsql_local(service_source_dir, pgsql_local_create):
    databases = discover.find_schemas('organizations', [service_source_dir / 'postgresql/schemas'])
    return pgsql_local_create(list(databases.values()))
pytest_plugins = ['pytest_userver.plugins.core', 'pytest_userver.plugins.postgresql']
# SERVICE_TESTS_END

@pytest.fixture(scope='session')
async def service_http_ping_url(monitor_baseurl):
    # Готовность проверяется на внутреннем listener до первого запроса testsuite.
    return monitor_baseurl.rstrip('/') + '/ping'

import sys
from pathlib import Path
from uuid import uuid4

from tests.support.auth_tokens import ISSUER, SigningKey

ORGANIZATION = '11111111-1111-4111-8111-111111111111'
OTHER_ORGANIZATION = '22222222-2222-4222-8222-222222222222'
USERVER_CONFIG_HOOKS = ['configure_auth']


@pytest.fixture(scope='session')
def auth_key(tmp_path_factory):
    return SigningKey(tmp_path_factory.mktemp('jwt') / 'key.pem', 'organizations')


@pytest.fixture(scope='session')
def configure_auth(mockserver_info):
    def patch(config, config_vars):
        components = config['components_manager']['components']
        components['platform-auth'].update({
            'issuer': ISSUER, 'jwks-url': mockserver_info.url('/auth/jwks'), 'allow-http': True,
        })
        components['service-token-provider'].update({
            'endpoint': mockserver_info.url('/auth/token'), 'allow-http': True,
        })
        components['keycloak-users'].update({
            'base-url': mockserver_info.url('/admin/realms/broker').rstrip('/'), 'allow-http': True,
        })
    return patch


@pytest.fixture(autouse=True)
def identity_provider(mockserver, auth_key):
    users = {}
    calls = []
    state = {'users': users, 'calls': calls, 'unavailable': False}

    @mockserver.json_handler('/auth/jwks')
    def jwks(request):
        return {'keys': [auth_key.jwk]}

    @mockserver.json_handler('/auth/token')
    def token(request):
        return {'access_token': 'test-management-token'}

    @mockserver.handler('/admin/realms/broker/users')
    def manage(request):
        calls.append(request.method)
        assert request.headers['Authorization'] == 'Bearer test-management-token'
        if state['unavailable']:
            return mockserver.make_response(status=503)
        if request.method == 'GET':
            username = request.query['username']
            return mockserver.make_response(json=[users[username]] if username in users else [])
        value = request.json
        username = value['username']
        if username in users:
            return mockserver.make_response(status=409)
        value['id'] = str(uuid4())
        users[username] = value
        return mockserver.make_response(status=201)

    return state


def insert_member(cursor, organization, subject, role, *, username=None, name=None, enabled=True):
    username = username or subject
    query = (Path(__file__).resolve().parents[1] / 'postgresql/queries/add_member.sql').read_text()
    query = query.replace('$1', '%(organization)s').replace('$2', '%(issuer)s').replace('$3', '%(member)s')
    cursor.execute(query, {
        'organization': organization,
        'issuer': ISSUER,
        'member': (subject, username, name or subject, username + '@example.test', role, True, 1,
                   None, None, None),
    })
    if not enabled:
        cursor.execute(
            'UPDATE memberships SET enabled = false WHERE organization_id=%s AND issuer=%s AND subject=%s',
            (organization, ISSUER, subject),
        )


@pytest.fixture(autouse=True)
def memberships(pgsql):
    import sys
    sys.path.insert(0, str(Path(__file__).resolve().parents[3] / 'scripts'))
    from permission_catalog import load
    from tests.support.permission_fixtures import register_sql
    catalog = load(Path(__file__).resolve().parents[3])
    with pgsql['db_1'].cursor() as cursor:
        for owner in sorted({value['owner'] for value in catalog.values()}):
            cursor.execute(register_sql(owner, 'test', catalog).replace('BEGIN;', '').replace('COMMIT;', ''))
        cursor.execute('INSERT INTO organizations (id, name) VALUES (%s,%s),(%s,%s)',
                       (ORGANIZATION, 'Первая организация', OTHER_ORGANIZATION, 'Другая организация'))
        default_roles = (Path(__file__).resolve().parents[1] / 'postgresql/queries/create_default_roles.sql').read_text().replace('$1::UUID', '%s::UUID')
        for organization in [ORGANIZATION, OTHER_ORGANIZATION]:
            cursor.execute(default_roles, (organization, organization, organization))
        for organization, subject, role in [
            (ORGANIZATION, 'org-admin', 'administrator'),
            (ORGANIZATION, 'viewer', 'viewer'),
            (OTHER_ORGANIZATION, 'other-admin', 'administrator'),
        ]:
            insert_member(cursor, organization, subject, role)


@pytest.fixture
def api(service_client, auth_key):
    class Client:
        async def call(self, path, body=None, *, user='platform', expected=200, roles=None, client='broker-web'):
            roles = roles if roles is not None else (['platform.admin'] if user == 'platform' else [])
            token = auth_key.token(user, claims={
                'azp': client, 'aud': 'broker-logistics', 'resource_access': {'broker-logistics': {'roles': roles}},
            })
            response = await service_client.request(
                'GET' if body is None else 'POST', path, json=body,
                headers={'Authorization': 'Bearer ' + token},
            )
            if expected is None:
                return response.status, response.json()
            assert response.status == expected, response.text
            return response.json()
    return Client()
