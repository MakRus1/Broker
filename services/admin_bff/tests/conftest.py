# SERVICE_TESTS_BEGIN
import pytest
pytest_plugins = ['pytest_userver.plugins.core']
# SERVICE_TESTS_END

@pytest.fixture(scope='session')
async def service_http_ping_url(monitor_baseurl):
    # Готовность проверяется на внутреннем listener до первого запроса testsuite.
    return monitor_baseurl.rstrip('/') + '/ping/ready'


import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tests/support"))
from auth_tokens import ISSUER, SigningKey

USERVER_CONFIG_HOOKS = ['configure_admin']


@pytest.fixture(scope='session')
def auth_key(tmp_path_factory):
    return SigningKey(tmp_path_factory.mktemp('bff-auth') / 'key.pem', 'bff-key')


@pytest.fixture(scope='session')
def configure_admin(mockserver_info, service_source_dir):
    def patch(config, config_vars):
        config_vars['auth-issuer'] = ISSUER
        config_vars['auth-jwks-url'] = mockserver_info.url('/auth/jwks')
        config_vars['admin-token-url'] = mockserver_info.url('/auth/token')
        config_vars['admin-revoke-url'] = mockserver_info.url('/auth/revoke')
        config_vars['admin-assets-dir'] = str(service_source_dir / 'tests/static')
        config_vars['client-organizations-base-url'] = mockserver_info.base_url.rstrip('/')
        config_vars['client-config_admin-base-url'] = mockserver_info.base_url.rstrip('/')
        config_vars['client-shipments-base-url'] = mockserver_info.base_url
        config_vars['client-transport_locations-base-url'] = mockserver_info.base_url.rstrip('/')
        config_vars['client-transportations-base-url'] = mockserver_info.base_url.rstrip('/')
    return patch


@pytest.fixture(autouse=True)
def oidc(mockserver, auth_key):
    state = {'nonce': '', 'challenge': '', 'claims': {}, 'id_claims': {}, 'token_status': 200,
             'grants': [], 'expires_in': 300, 'codes': []}

    @mockserver.json_handler('/auth/jwks')
    def keys(request):
        return {'keys': [auth_key.jwk]}

    @mockserver.handler('/auth/token')
    async def token(request):
        import hashlib, base64
        from urllib.parse import parse_qs
        form = {name: values[0] for name, values in parse_qs(request.get_data().decode()).items()}
        grant = form['grant_type']
        state['grants'].append(grant)
        assert form['client_id'] == 'broker-admin'
        if grant == 'authorization_code':
            assert form['code'] not in state['codes']
            state['codes'].append(form['code'])
            challenge = base64.urlsafe_b64encode(hashlib.sha256(form['code_verifier'].encode()).digest()).rstrip(b'=').decode()
            assert challenge == state['challenge']
            assert form['redirect_uri'] == 'http://localhost:13080/auth/callback'
        else:
            assert grant == 'refresh_token'
            assert form['refresh_token'] == 'server-only-refresh'
        claims = {'azp': 'broker-admin', 'preferred_username': 'author', **state['claims']}
        identity = {'typ': 'ID', 'aud': 'broker-admin', 'azp': 'broker-admin',
                    'nonce': state['nonce'], **state['id_claims']}
        return mockserver.make_response(status=state['token_status'], json={
            'access_token': auth_key.token(claims=claims), 'id_token': auth_key.token(claims=identity),
            'refresh_token': 'server-only-refresh', 'expires_in': state['expires_in']})

    @mockserver.json_handler('/auth/revoke')
    def revoke(request):
        return {}

    return state


@pytest.fixture
def login(service_client, oidc):
    async def perform():
        from urllib.parse import urlparse, parse_qs
        started = await service_client.get('/auth/login', allow_redirects=False)
        assert started.status == 302
        parameters = parse_qs(urlparse(started.headers['Location']).query)
        oidc['nonce'] = parameters['nonce'][0]
        oidc['challenge'] = parameters['code_challenge'][0]
        state = parameters['state'][0]
        cookie = 'broker_login=' + started.cookies['broker_login'].value
        callback = await service_client.get('/auth/callback', params={'state': state, 'code': state},
                                             headers={'Cookie': cookie}, allow_redirects=False)
        return callback
    return perform


@pytest.fixture
def browser(service_client, login):
    class Browser:
        cookie = ''
        session = None

        async def sign_in(self):
            callback = await login()
            assert callback.status == 302, callback.text
            self.cookie = 'broker_admin=' + callback.cookies['broker_admin'].value
            response = await self.get('/api/session')
            assert response.status == 200
            self.session = response.json()
            return self.session

        async def get(self, path):
            return await service_client.get(path, headers={'Cookie': self.cookie})

        async def post(self, path, body, headers=None):
            values = {'Cookie': self.cookie, 'Origin': 'http://localhost:13080',
                      'X-CSRF-Token': self.session['csrf_token']}
            return await service_client.post(path, json=body, headers={**values, **(headers or {})})
    return Browser()


@pytest.fixture(autouse=True)
def platform_access(mockserver):
    from platform_access import install
    return install(mockserver)
