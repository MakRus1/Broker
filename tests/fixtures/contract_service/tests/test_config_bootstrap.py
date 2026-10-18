"""Настоящий перезапуск процесса с дисковым кешем и недоступным configs."""
import asyncio
from copy import deepcopy
import json
import time

import aiohttp
import pytest
import yaml


@pytest.mark.parametrize('case,expected', [
    ('missing', 503), ('fresh', 204), ('expired', 503), ('incompatible', 503),
    ('corrupted', 503), ('bad-metadata', 503), ('cache-forbidden', 503), ('builtin-allowed', 204),
])
async def test_bootstrap_without_config_server(service_binary, service_config, tmp_path, unused_tcp_port_factory, mockserver, case, expected):
    @mockserver.handler('/offline-configs/configs/values')
    def unavailable(request):
        return mockserver.make_response(status=503)

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
    policy = components['platform-config-readiness']['rules'][0]
    policy['allow-cache'] = case != 'cache-forbidden'
    policy['allow-builtin'] = case == 'builtin-allowed'
    cache = tmp_path / 'cache.json'
    components['dynamic-config']['fs-cache-path'] = str(cache)
    components['dynamic-config']['updates-enabled'] = True
    if case not in {'missing', 'builtin-allowed'}:
        value = {
            'BROKER_HTTP_CLIENT_CONFIG': {'**default**': {'timeout': 100, 'max_retries': 0}},
            'BROKER_CONFIG_DELIVERY_META': {
                'process': 'previous-process',
                'received_at_ms': int((time.time() - (60 if case == 'expired' else 0)) * 1000),
                'schemas': {'BROKER_HTTP_CLIENT_CONFIG': '0' * 64 if case == 'incompatible' else policy['schema-hash']},
            },
        }
        if case == 'bad-metadata':
            value['BROKER_CONFIG_DELIVERY_META'] = {'process': 123}
        cache.write_text('{' if case == 'corrupted' else json.dumps(value))
    path = tmp_path / 'config.yaml'
    path.write_text(yaml.safe_dump(config))
    with (tmp_path / 'service.log').open('w') as log:
        process = await asyncio.create_subprocess_exec(str(service_binary), '--config', str(path), stdout=log, stderr=log)
        try:
            async with aiohttp.ClientSession() as client:
                for _ in range(100):
                    if process.returncode is not None:
                        pytest.fail((tmp_path / 'service.log').read_text())
                    try:
                        response = await client.get(f'http://127.0.0.1:{monitor}/ping')
                        if response.status == 200:
                            readiness = await client.get(f'http://127.0.0.1:{monitor}/ping/ready')
                            if readiness.status in {200, 503} and 'service is not running' not in await readiness.text():
                                break
                    except aiohttp.ClientConnectionError:
                        pass
                    await asyncio.sleep(0.05)
                else:
                    pytest.fail((tmp_path / 'service.log').read_text())
                assert (await client.get(f'http://127.0.0.1:{monitor}/ping/ready')).status == (200 if expected == 204 else 503)
                assert (await client.get(f'http://127.0.0.1:{port}/items/1')).status == expected
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
