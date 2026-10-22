"""Авторизация проверяется настоящими RSA-подписями, мокается только JWKS-сервер."""

import asyncio
import time
import pytest

from auth_tokens import ISSUER
from test_configs import terms
from test_drafts import create


async def call(client, token=None, path='/admin/configs/list', **kwargs):
    headers = {} if token is None else {'Authorization': 'Bearer ' + token}
    return await client.get(path, headers=headers, **kwargs)


@pytest.mark.parametrize('token', [None, '', 'malformed', 'e30.e30.fake'])
async def test_missing_or_malformed_token(monitor_client, token):
    response = await call(monitor_client, token)
    assert response.status == 401
    assert response.headers['WWW-Authenticate'] == 'Bearer'
    assert response.json()['error'] == 'unauthorized'


@pytest.mark.parametrize(
    'claims',
    [
        {'exp': None},
        {'iat': None},
        {'sub': None},
        {'iss': 'http://untrusted'},
        {'aud': 'another-api'},
        {'exp': 1},
        {'iat': 1, 'exp': 0},
        {'iat': int(time.time()) + 1000},
        {'nbf': int(time.time()) + 1000},
        {'sub': ''},
        {'typ': 'ID'},
    ],
)
async def test_invalid_claims(monitor_client, access_token, claims):
    assert (await call(monitor_client, access_token(claims=claims))).status == 401


@pytest.mark.parametrize(
    'header',
    [
        {'alg': 'none'},
        {'alg': 'HS256'},
        {'crit': []},
        {'jku': 'http://untrusted/keys'},
        {'kid': 'unknown'},
    ],
)
async def test_invalid_header(monitor_client, access_token, header):
    assert (await call(monitor_client, access_token(header=header))).status == 401


async def test_invalid_signature(monitor_client, auth_keys):
    token = auth_keys[1].token(header={'kid': auth_keys[0].kid})
    assert (await call(monitor_client, token)).status == 401


@pytest.mark.parametrize('roles', [[], ['configs.write'], ['configs.approve'], ['admin']])
async def test_read_permission_required(monitor_client, access_token, roles):
    assert (await call(monitor_client, access_token(roles=roles))).status == 403


async def test_no_roles_is_forbidden(monitor_client, access_token):
    assert (await call(monitor_client, access_token(claims={'resource_access': {}}))).status == 403


@pytest.mark.parametrize(
    'role,path',
    [('configs.read', 'create'), ('configs.approve', 'edit'), ('configs.write', 'approve')],
)
async def test_mutation_permissions_before_body(monitor_client, access_token, role, path):
    response = await monitor_client.post(
        '/admin/configs/drafts/' + path,
        json={},
        headers={'Authorization': 'Bearer ' + access_token(roles=[role])},
    )
    assert response.status == 403


async def test_authenticated_actor_cannot_be_overridden(admin_client, monitor_client, access_token):
    response = await admin_client.post(
        '/admin/configs/drafts/create',
        json={
            'actor': 'reviewer',
            'config_name': 'SHIPMENT_TERMS_CONFIG',
            'expected_config_version': 0,
            'description': 'Подмена автора',
            'value': terms(),
        },
    )
    assert response.status == 400
    draft = await create(admin_client)
    assert draft['author'] == ISSUER + '#author'
    token = access_token(claims={'preferred_username': 'another-name'})
    response = await monitor_client.post(
        '/admin/configs/drafts/approve',
        json={'id': draft['id'], 'expected_version': draft['version']},
        headers={'Authorization': 'Bearer ' + token},
    )
    assert response.status == 403


async def test_cached_keys_survive_outage(monitor_client, access_token, auth_jwks):
    assert (await call(monitor_client, access_token())).status == 200
    auth_jwks['status'] = 503
    for _ in range(70):
        await call(monitor_client, access_token(header={'kid': 'outage-key'}))
        if auth_jwks['failures']:
            break
        await asyncio.sleep(0.1)
    assert auth_jwks['failures'] > 0
    assert (await call(monitor_client, access_token())).status == 200


async def test_key_rotation(monitor_client, auth_keys, auth_jwks):
    assert (await call(monitor_client, auth_keys[0].token())).status == 200
    auth_jwks['keys'] = [key.jwk for key in auth_keys]
    token = auth_keys[1].token()
    for _ in range(70):
        response = await call(monitor_client, token)
        if response.status == 200:
            break
        assert response.status == 401
        await asyncio.sleep(0.1)
    assert response.status == 200


async def test_public_config_delivery_needs_no_user_token(service_client):
    assert (await service_client.post('/configs/values', json={})).status == 200


async def test_bearer_scheme_case_insensitive(monitor_client, access_token):
    response = await monitor_client.get(
        '/admin/configs/list', headers={'Authorization': 'bEaReR ' + access_token()}
    )
    assert response.status == 200


@pytest.fixture(scope='session')
def configure_parameterized_auth():
    def patch(config, config_vars):
        components = config['components_manager']['components']
        original = '/admin/configs/list'
        route = '/admin/configs/{category}/list'
        handler = next(
            value
            for value in components.values()
            if isinstance(value, dict) and value.get('path') == original
        )
        handler['path'] = route
        permissions = components['platform-auth']['permissions']
        permissions[route] = permissions.pop(original)

    return patch


@pytest.mark.uservice_oneshot(config_hooks=['configure_parameterized_auth'])
async def test_permission_uses_route_template(monitor_client, access_token):
    for category in ('first', 'second'):
        path = '/admin/configs/' + category + '/list'
        assert (await call(monitor_client, path=path)).status == 401
        assert (await call(monitor_client, access_token(roles=[]), path=path)).status == 403
        assert (await call(monitor_client, access_token(), path=path)).status == 200


@pytest.mark.parametrize('logistics_roles', [[], ['organizations.manage'], ['platform.admin']])
async def test_organization_and_platform_roles_do_not_grant_config_access(
    monitor_client, access_token, logistics_roles
):
    token = access_token(
        claims={
            'resource_access': {
                'broker-logistics': {'roles': logistics_roles},
            }
        }
    )
    assert (await call(monitor_client, token)).status == 403
    response = await monitor_client.post(
        '/admin/configs/drafts/create', json={}, headers={'Authorization': 'Bearer ' + token}
    )
    assert response.status == 403
