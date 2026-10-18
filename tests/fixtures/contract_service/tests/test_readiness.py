"""Готовность HTTP зависит от подтвержденного обязательного конфига."""
import pytest
from testsuite.utils import http

KEY = 'BROKER_HTTP_CLIENT_CONFIG'
UPDATER = 'dynamic-config-client-updater'


def settings():
    return {'**default**': {'timeout': 100, 'max_retries': 0}}


async def test_missing_config_blocks_and_recovers(service_client, monitor_client, dynamic_config):
    dynamic_config.remove_values([KEY])
    assert (await service_client.get('/items/1')).status == 503
    assert (await monitor_client.get('/ping')).status == 200
    assert (await monitor_client.get('/ping/ready')).status == 503
    dynamic_config.set_values({KEY: settings()})
    assert (await service_client.get('/items/1')).status == 204
    assert (await monitor_client.get('/ping/ready')).status == 200


@pytest.mark.now('2026-10-18T12:00:00+0000')
async def test_expiry_and_unchanged_confirmation(service_client, monitor_client, mocked_time):
    await service_client.invalidate_caches(cache_names=[UPDATER])
    assert (await service_client.get('/items/1')).status == 204
    mocked_time.sleep(6)
    await service_client.tests_control(invalidate_caches=False)
    assert (await service_client.get('/items/1')).status == 503
    assert (await monitor_client.get('/ping')).status == 200
    assert (await monitor_client.get('/ping/ready')).status == 503
    # Значение не менялось: успешная полная проверка все равно продлевает свежесть.
    await service_client.invalidate_caches(cache_names=[UPDATER])
    assert (await service_client.get('/items/1')).status == 204
    assert (await monitor_client.get('/ping/ready')).status == 200


@pytest.mark.now('2026-10-18T12:00:00+0000')
async def test_failed_update_does_not_refresh_age(
    service_client, monitor_client, dynamic_config, mocked_time,
):
    await service_client.invalidate_caches(cache_names=[UPDATER])
    assert (await service_client.get('/items/1')).status == 204
    dynamic_config.set_values({KEY: {'**default**': {'timeout': -1, 'max_retries': 0}}})
    with pytest.raises(http.HttpResponseError):
        await service_client.update_server_state()
    # Рабочее значение сохраняется, ошибочная публикация не обновляет timestamp.
    assert (await monitor_client.get('/ping/ready')).status == 200
    mocked_time.sleep(6)
    await service_client.tests_control(invalidate_caches=False)
    assert (await monitor_client.get('/ping/ready')).status == 503
    dynamic_config.set_values({KEY: settings()})
    assert (await service_client.get('/items/1')).status == 204
