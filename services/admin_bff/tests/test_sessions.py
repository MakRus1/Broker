"""Вход проверяет настоящие подписи JWT; мокается только доверенный провайдер."""
import asyncio
import pytest


async def test_anonymous(service_client):
    response = await service_client.get('/api/session')
    assert response.status == 401
    assert response.headers['Cache-Control'] == 'no-store'


async def test_login_cookie_and_server_only_tokens(browser, login):
    callback = await login()
    assert callback.status == 302
    cookie = callback.cookies['broker_admin']
    assert cookie['httponly']
    assert cookie['samesite'] == 'Lax'
    assert cookie['path'] == '/'
    assert callback.cookies['broker_login']['max-age'] == '0'
    session = await browser.sign_in()
    assert session['actor'] == 'http://broker-test-issuer#author'
    assert 'access_token' not in session and 'refresh_token' not in session
    assert len(session['csrf_token']) >= 40


@pytest.mark.parametrize('kind', ['missing-cookie', 'wrong-state', 'missing-code', 'duplicate-code'])
async def test_invalid_callback(service_client, kind):
    from urllib.parse import urlparse, parse_qs
    started = await service_client.get('/auth/login', allow_redirects=False)
    state = parse_qs(urlparse(started.headers['Location']).query)['state'][0]
    cookie = 'broker_login=' + started.cookies['broker_login'].value
    params = [('state', state), ('code', 'one')]
    headers = {'Cookie': cookie}
    cookies = {}
    if kind == 'missing-cookie':
        headers = {}
        cookies = {'broker_login': ''}
    elif kind == 'wrong-state':
        params[0] = ('state', 'other')
    elif kind == 'missing-code':
        params.pop()
    else:
        params.append(('code', 'two'))
    response = await service_client.get('/auth/callback', params=params, headers=headers, cookies=cookies)
    assert response.status == 400


async def test_callback_replay(service_client, login, oidc):
    callback = await login()
    assert callback.status == 302
    state = oidc['codes'][0]
    response = await service_client.get('/auth/callback', params={'state': state, 'code': state},
                                         headers={'Cookie': 'broker_login=' + state})
    assert response.status == 401


@pytest.mark.parametrize('claims', [{'nonce': 'wrong'}, {'aud': 'other'}, {'azp': 'other'},
                                   {'typ': 'Bearer'}, {'exp': 1}, {'sub': 'different'}])
async def test_invalid_identity_token(login, oidc, claims):
    oidc['id_claims'] = claims
    callback = await login()
    assert callback.status == 401
    assert callback.cookies['broker_admin']['max-age'] == '0'


async def test_foreign_access_token(login, oidc):
    oidc['claims'] = {'azp': 'other'}
    assert (await login()).status == 401


async def test_refresh_serialized(browser, oidc):
    oidc['expires_in'] = 1
    await browser.sign_in()
    oidc['expires_in'] = 300
    before = len(oidc['grants'])
    responses = await asyncio.gather(browser.get('/api/session'), browser.get('/api/session'))
    assert all(response.status == 200 for response in responses)
    assert oidc['grants'][before:] == ['refresh_token']


@pytest.mark.parametrize('headers', [{'Origin': 'https://evil.test'}, {'X-CSRF-Token': ''}])
async def test_csrf(browser, headers):
    await browser.sign_in()
    assert (await browser.post('/api/logout', {}, headers)).status == 403


async def test_logout_invalidates_session(browser):
    await browser.sign_in()
    response = await browser.post('/api/logout', {})
    assert response.status == 200
    assert response.cookies['broker_admin']['max-age'] == '0'
    assert (await browser.get('/api/session')).status == 401


async def test_static_html_and_csp(service_client):
    response = await service_client.get('/')
    assert response.status == 200
    assert 'Админка' in response.text
    assert "frame-ancestors 'none'" in response.headers['Content-Security-Policy']
    assert response.headers['X-Content-Type-Options'] == 'nosniff'


async def test_lost_refresh_cookie_is_cleared(browser, oidc):
    oidc['expires_in'] = 1
    await browser.sign_in()
    oidc['token_status'] = 400
    response = await browser.get('/api/session')
    assert response.status == 401
    assert response.cookies['broker_admin']['max-age'] == '0'


async def test_provider_down(browser, oidc):
    oidc['expires_in'] = 1
    await browser.sign_in()
    oidc['token_status'] = 503
    response = await browser.get('/api/session')
    assert response.status == 503
    assert response.json()['error'] == 'auth_unavailable'


async def test_login_secrets_are_not_logged(service_client, login, oidc):
    import json

    async with service_client.capture_logs() as capture:
        callback = await login()
        assert callback.status == 302

    records = capture.select()
    assert records
    serialized = json.dumps(records)
    assert oidc['codes'][-1] not in serialized
    assert callback.cookies['broker_admin'].value not in serialized
    assert 'server-only-refresh' not in serialized
