"""Выдача опубликованных значений и отделение административного API."""
import pytest


def terms(enabled=False, days=30):
    return {'clauses': [], 'default': {'is_enabled': enabled, 'max_days': days}}


async def current(service_client):
    response = await service_client.post('/configs/values', json={})
    assert response.status == 200
    return response.json()


async def publish_one(client, name, value):
    opened = await client.post('/admin/configs/current', json={'config_name': name})
    assert opened.status == 200
    created = await client.post('/admin/configs/drafts/create', json={
        'actor': 'author', 'config_name': name, 'expected_config_version': opened.json()['version'],
        'description': 'Обновление конфига', 'value': value})
    assert created.status == 201
    draft = created.json()
    response = await client.post('/admin/configs/drafts/approve', json={
        'actor': 'reviewer', 'id': draft['id'], 'expected_version': draft['version']})
    assert response.status == 200
    return response.json()


async def test_values_and_history(service_client, monitor_client):
    initial = await current(service_client)
    assert initial['revision'] == 0
    published = await publish_one(monitor_client, 'SHIPMENT_TERMS_CONFIG', terms(True, 60))
    values = await current(service_client)
    assert values['revision'] == published['published_revision']
    assert values['updated_at'] > initial['updated_at']
    assert values['configs']['SHIPMENT_TERMS_CONFIG'] == terms(True, 60)
    history = await monitor_client.get('/admin/configs/history')
    assert len(history.json()["items"]) == 1


@pytest.mark.parametrize('name,value', [
    ('UNKNOWN', {}),
    ('SHIPMENT_TERMS_CONFIG', {'clauses': [], 'default': {'max_days': 30}}),
    ('SHIPMENT_TERMS_CONFIG', terms(True, 0)),
    ('BROKER_HTTP_CLIENT_CONFIG', {'**default**': {'timeout': 0, 'max_retries': 0}}),
    ('BROKER_HTTP_CLIENT_CONFIG', {'**default**': {'timeout': 100, 'max_retries': 0}, '/unknown': {}}),
])
async def test_invalid_draft_is_atomic(service_client, monitor_client, name, value):
    before = await current(service_client)
    response = await monitor_client.post('/admin/configs/drafts/create', json={
        'actor': 'author', 'config_name': name, 'description': 'Проверка',
        'expected_config_version': 0, 'value': value})
    assert response.status == 400
    assert await current(service_client) == before
    assert (await monitor_client.post('/admin/configs/drafts/list', json={})).json()['items'] == []


async def test_values_filter_and_unknown_native_configs(service_client):
    response = await service_client.post('/configs/values', json={
        'ids': ['SHIPMENT_TERMS_CONFIG', 'USERVER_UNKNOWN_NATIVE_CONFIG'], 'updated_since': ''})
    assert response.status == 200
    assert set(response.json()['configs']) == {'SHIPMENT_TERMS_CONFIG'}


async def test_required_http_defaults_and_inheritance(monitor_client):
    await publish_one(monitor_client, 'BROKER_HTTP_CLIENT_CONFIG', {
        '**default**': {'timeout': 100, 'max_retries': 1}, '/hello': {'timeout': 500}})


@pytest.mark.parametrize('path', ['/configs/publish', '/configs/rollback', '/configs/history',
                                  '/admin/configs/publish', '/admin/configs/rollback'])
async def test_no_direct_write_routes(service_client, monitor_client, path):
    for client in [service_client, monitor_client]:
        response = await client.post(path, json={})
        assert response.status == 404


async def test_history_is_internal(service_client):
    response = await service_client.get('/admin/configs/history')
    assert response.status == 404


async def test_native_userver_request_metadata(service_client):
    response = await service_client.post('/configs/values', json={
        'service': 'diagnostics', 'stage_name': 'local',
        'service_overrides': ['diagnostics'], 'is_prestable': True,
        'circuit': 'local',
    })
    assert response.status == 200
    assert 'SHIPMENT_TERMS_CONFIG' in response.json()['configs']


async def test_native_userver_untyped_json(service_client):
    response = await service_client.post('/configs/values', data='{"service":"diagnostics"}',
                                         headers={'Content-Type': 'application/x-www-form-urlencoded'})
    assert response.status == 200
    assert 'configs' in response.json()
