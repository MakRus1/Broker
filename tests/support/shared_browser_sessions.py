from contextlib import asynccontextmanager
import asyncio
from urllib.parse import parse_qs, urlparse

import aiohttp
import pytest
import yaml


@pytest.fixture
def bff_replica(service_client, service_config_path_temp, service_binary, tmp_path, unused_tcp_port_factory):
    config = yaml.safe_load(service_config_path_temp.read_text())
    components = config['components_manager']['components']
    port, monitor_port = unused_tcp_port_factory(), unused_tcp_port_factory()
    components['server']['listener']['port'] = port
    components['server']['listener-monitor']['port'] = monitor_port
    replica_config = tmp_path / 'replica.yaml'
    replica_config.write_text(yaml.safe_dump(config))

    @asynccontextmanager
    async def start():
        with (tmp_path / 'replica.log').open('ab') as log:
            process = await asyncio.create_subprocess_exec(
                str(service_binary), '--config', str(replica_config), stdout=log, stderr=log,
            )
            try:
                async with aiohttp.ClientSession(base_url=f'http://localhost:{port}') as client:
                    async with aiohttp.ClientSession() as monitor:
                        async with asyncio.timeout(15):
                            while True:
                                if process.returncode is not None:
                                    pytest.fail('Вторая реплика BFF завершилась при запуске')
                                try:
                                    async with monitor.get(f'http://127.0.0.1:{monitor_port}/ping') as reply:
                                        if reply.status == 200:
                                            break
                                except aiohttp.ClientConnectionError:
                                    pass
                                await asyncio.sleep(0.05)
                    yield client
            finally:
                if process.returncode is None:
                    process.terminate()
                    await asyncio.wait_for(process.wait(), timeout=10)

    return start


async def assert_shared_sessions(service_client, bff_replica, oidc, *, login_cookie, session_cookie, origin):
    started = await service_client.get('/auth/login', allow_redirects=False)
    query = parse_qs(urlparse(started.headers['Location']).query)
    state = query['state'][0]
    oidc.update(nonce=query['nonce'][0], challenge=query['code_challenge'][0], expires_in=10)
    login_headers = {'Cookie': f'{login_cookie}={started.cookies[login_cookie].value}'}
    callback = {'state': state, 'code': state}

    async with bff_replica() as replica:
        async with replica.get('/auth/callback', params=callback, headers=login_headers,
                               allow_redirects=False) as completed:
            assert completed.status == 302
            session_headers = {'Cookie': f'{session_cookie}={completed.cookies[session_cookie].value}'}

        replay = await service_client.get('/auth/callback', params=callback, headers=login_headers,
                                          allow_redirects=False)
        assert replay.status == 401

    # Перезапускаем реплику перед использованием её сессии.
    oidc.update(expires_in=300, refresh_delay=0.2)
    async with bff_replica() as replica:
        primary, secondary = await asyncio.gather(
            service_client.get('/api/session', headers=session_headers),
            replica.get('/api/session', headers=session_headers),
        )
        try:
            assert primary.status == secondary.status == 200
            session = primary.json()
            assert session == await secondary.json()
            assert oidc['grants'] == ['authorization_code', 'refresh_token']
        finally:
            secondary.release()

        async with replica.post('/api/logout', json={}, headers={
            **session_headers, 'Origin': origin,
            'X-CSRF-Token': session['csrf_token'],
        }) as logout:
            assert logout.status == 200

    assert (await service_client.get('/api/session', headers=session_headers)).status == 401
