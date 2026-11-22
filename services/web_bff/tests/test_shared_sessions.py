from tests.support.shared_browser_sessions import assert_shared_sessions, bff_replica


async def test_login_refresh_and_logout_across_replicas(service_client, bff_replica, oidc):
    await assert_shared_sessions(
        service_client, bff_replica, oidc,
        login_cookie='broker_web_login', session_cookie='broker_web', origin='http://localhost:14080',
    )


async def test_revisited_login_callback_returns_authenticated_user_to_app(browser, service_client, oidc):
    await browser.sign_in()
    before = dict(oidc)
    response = await service_client.get('/auth/callback?state=consumed&code=consumed',
                                        headers={'Cookie': browser.cookie}, allow_redirects=False)
    assert response.status == 302
    assert response.headers['Location'] == '/'
    assert oidc == before
    assert (await browser.get('/api/session')).status == 200


async def test_revisited_login_callback_does_not_authenticate_invalid_session(service_client):
    response = await service_client.get('/auth/callback?state=consumed&code=consumed',
                                        headers={'Cookie': 'broker_web=unknown'}, allow_redirects=False)
    assert response.status == 400
    assert response.json()['error'] == 'invalid_login'
