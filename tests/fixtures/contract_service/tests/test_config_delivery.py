"""Ошибки доставки не заменяют принятый снимок и не блокируют запросы."""
import pytest
from testsuite.utils import http

KEY = 'BROKER_HTTP_CLIENT_CONFIG'
UPDATER = 'dynamic-config-client-updater'


def settings():
    return {'**default**': {'timeout': 100, 'max_retries': 0}}


async def test_incomplete_update_preserves_snapshot(
    service_client, monitor_client, dynamic_config,
):
    assert (await service_client.get('/items/1')).status == 204
    dynamic_config.remove_values([KEY])
    with pytest.raises(http.HttpResponseError):
        await service_client.update_server_state()
    assert (await service_client.get('/items/1', testsuite_skip_prepare=True)).status == 204
    assert (await monitor_client.get('/ping')).status == 200
    dynamic_config.set_values({KEY: settings()})
    dynamic_config.switch_to_static_default(KEY)
    assert (await service_client.get('/items/1')).status == 204


@pytest.mark.now('2026-10-18T12:00:00+0000')
async def test_invalid_update_preserves_snapshot(
    service_client, monitor_client, dynamic_config, mocked_time,
):
    await service_client.invalidate_caches(cache_names=[UPDATER])
    assert (await service_client.get('/items/1')).status == 204
    dynamic_config.set_values({KEY: {'**default**': {'timeout': -1, 'max_retries': 0}}})
    with pytest.raises(http.HttpResponseError):
        await service_client.update_server_state()
    # Ошибка обновления не блокирует работу даже спустя время.
    assert (await monitor_client.get('/ping')).status == 200
    mocked_time.sleep(6)
    await service_client.tests_control(invalidate_caches=False)
    assert (await monitor_client.get('/ping')).status == 200
    assert (await service_client.get('/items/1', testsuite_skip_prepare=True)).status == 204
    dynamic_config.set_values({KEY: settings()})
    assert (await service_client.get('/items/1')).status == 204
