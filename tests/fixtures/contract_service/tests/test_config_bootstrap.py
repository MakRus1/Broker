"""Запуск процесса при недоступном Configs и восстановлении доставки."""
import asyncio
from copy import deepcopy

import aiohttp
import pytest
import yaml


async def test_startup_waits_for_configs(service_binary, service_config, tmp_path, unused_tcp_port_factory, mockserver):
    unavailable_request = asyncio.Event()
    configs_available = False

    @mockserver.handler('/offline-configs/configs/values')
    def configs_values(request):
        if not configs_available:
            unavailable_request.set()
            return mockserver.make_response(status=503)
        return mockserver.make_response(json={
            'configs': {
                'BROKER_HTTP_CLIENT_CONFIG': {'**default**': {'timeout': 100, 'max_retries': 0}},
            },
            'kill_switches_disabled': [
                key for key in request.json['ids'] if key != 'BROKER_HTTP_CLIENT_CONFIG'
            ],
            'updated_at': '2026-01-01T00:00:00Z',
        })

    config = deepcopy(service_config)
    components = config['components_manager']['components']
    port, monitor = unused_tcp_port_factory(), unused_tcp_port_factory()
    components['server']['listener'] = {'address': '127.0.0.1', 'port': port, 'task_processor': 'main-task-processor'}
    components['server']['listener-monitor'] = {'address': '127.0.0.1', 'port': monitor, 'task_processor': 'main-task-processor'}
    components['testsuite-support'] = {}
    components['tests-control']['load-enabled'] = False
    components['dynamic-config-client']['config-url'] = mockserver.url('offline-configs').rstrip('/')
    components['dynamic-config-client']['http-timeout'] = '100ms'
    for name in ['dynamic-config-client', 'dynamic-config-client-updater', 'platform-config-delivery']:
        components[name]['load-enabled'] = True
    components['platform-config-startup']['load-enabled'] = True
    components['dynamic-config']['updates-enabled'] = True
    path = tmp_path / 'config.yaml'
    path.write_text(yaml.safe_dump(config))
    with (tmp_path / 'service.log').open('w') as log:
        process = await asyncio.create_subprocess_exec(str(service_binary), '--config', str(path), stdout=log, stderr=log)
        try:
            async with aiohttp.ClientSession() as client:
                await asyncio.wait_for(unavailable_request.wait(), timeout=5)
                try:
                    response = await client.get(f'http://127.0.0.1:{monitor}/ping')
                except aiohttp.ClientConnectionError:
                    pass
                else:
                    assert response.status == 500
                with pytest.raises(aiohttp.ClientConnectionError):
                    await client.get(f'http://127.0.0.1:{port}/items/1')
                unavailable_request.clear()
                configs_available = True
                for _ in range(100):
                    if process.returncode is not None:
                        pytest.fail((tmp_path / 'service.log').read_text())
                    try:
                        response = await client.get(f'http://127.0.0.1:{monitor}/ping')
                        if response.status == 200:
                            break
                    except aiohttp.ClientConnectionError:
                        pass
                    await asyncio.sleep(0.05)
                else:
                    pytest.fail((tmp_path / 'service.log').read_text())
                assert (await client.get(f'http://127.0.0.1:{port}/items/1')).status == 204
                configs_available = False
                await asyncio.wait_for(unavailable_request.wait(), timeout=5)
                assert (await client.get(f'http://127.0.0.1:{monitor}/ping')).status == 200
                assert (await client.get(f'http://127.0.0.1:{port}/items/1')).status == 204
                assert process.returncode is None
        finally:
            if process.returncode is None:
                process.terminate()
            try:
                await asyncio.wait_for(process.wait(), timeout=10)
            except asyncio.TimeoutError:
                process.kill()
                await process.wait()
                raise
