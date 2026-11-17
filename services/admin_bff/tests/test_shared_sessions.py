from tests.support.shared_browser_sessions import assert_shared_sessions, bff_replica


async def test_login_refresh_and_logout_across_replicas(service_client, bff_replica, oidc):
    await assert_shared_sessions(
        service_client, bff_replica, oidc,
        login_cookie='broker_login', session_cookie='broker_admin', origin='http://localhost:13080',
    )
