"""Публикация, валидация, конкуренция ревизий и откат JSON-конфигов."""
import asyncio
import pytest


def terms(enabled=False, days=30):
    return {'clauses': [], 'default': {'is_enabled': enabled, 'max_days': days}}


async def current(service_client):
    response = await service_client.post('/configs/values', json={})
    assert response.status == 200
    return response.json()


async def publish(monitor_client, revision, values):
    return await monitor_client.post('/configs/publish', json={
        'expected_revision': revision, 'configs': values})


async def test_publish_and_rollback(service_client, monitor_client):
    initial = await current(service_client)
    assert initial['revision'] == 0
    response = await publish(monitor_client, 0, {'SHIPMENT_TERMS_CONFIG': terms(True, 60)})
    assert response.status == 200
    first = response.json()['revision']
    values = await current(service_client)
    assert values['configs']['SHIPMENT_TERMS_CONFIG']['default']['max_days'] == 60
    assert values['updated_at'] > initial['updated_at']
    second = await publish(monitor_client, first, {'SHIPMENT_TERMS_CONFIG': terms(True, 90)})
    response = await monitor_client.post('/configs/rollback', json={
        'expected_revision': second.json()['revision'], 'revision': first})
    assert response.status == 200
    assert (await current(service_client))['configs']['SHIPMENT_TERMS_CONFIG']['default']['max_days'] == 60
    history = await monitor_client.get('/configs/history')
    assert len(history.json()) == 3


@pytest.mark.parametrize('value', [
    {'UNKNOWN': {}},
    {'SHIPMENT_TERMS_CONFIG': {'clauses': [], 'default': {'max_days': 30}}},
    {'SHIPMENT_TERMS_CONFIG': terms(True, 0)},
    {'BROKER_HTTP_CLIENT_CONFIG': {'**default**': {'timeout': 0, 'max_retries': 0}}},
    {'BROKER_HTTP_CLIENT_CONFIG': {'**default**': {'timeout': 100, 'max_retries': 0}, '/unknown': {}}},
])
async def test_invalid_publication_is_atomic(service_client, monitor_client, value):
    before = await current(service_client)
    response = await publish(monitor_client, before['revision'], value)
    assert response.status == 400
    assert await current(service_client) == before


async def test_concurrent_publications(service_client, monitor_client):
    revision = (await current(service_client))['revision']
    responses = await asyncio.gather(
        publish(monitor_client, revision, {'SHIPMENT_TERMS_CONFIG': terms(True, 60)}),
        publish(monitor_client, revision, {'SHIPMENT_TERMS_CONFIG': terms(True, 90)}),
    )
    assert sorted(response.status for response in responses) == [200, 409]


async def test_values_filter_and_unknown_native_configs(service_client):
    response = await service_client.post('/configs/values', json={
        'ids': ['SHIPMENT_TERMS_CONFIG', 'USERVER_UNKNOWN_NATIVE_CONFIG'], 'updated_since': ''})
    assert response.status == 200
    assert set(response.json()['configs']) == {'SHIPMENT_TERMS_CONFIG'}


async def test_required_http_defaults_and_inheritance(service_client, monitor_client):
    revision = (await current(service_client))['revision']
    response = await publish(monitor_client, revision, {'BROKER_HTTP_CLIENT_CONFIG': {
        '**default**': {'timeout': 100, 'max_retries': 1}, '/hello': {'timeout': 500}}})
    assert response.status == 200


@pytest.mark.parametrize('path', ['/configs/publish', '/configs/rollback', '/configs/history'])
async def test_administration_is_internal(service_client, path):
    response = await service_client.post(path, json={})
    assert response.status == 404
