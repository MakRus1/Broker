import pytest
from test_configs import publish_one, current


@pytest.mark.parametrize('key,value', [
    ('POSTGRES_DEFAULT_COMMAND_CONTROL', {'network_timeout_ms': 2000, 'statement_timeout_ms': 1000}),
    ('POSTGRES_QUERIES_COMMAND_CONTROL', {'directory-sync.railway': {'network_timeout_ms': 60000, 'statement_timeout_ms': 50000}, 'some_future_query': {'network_timeout_ms': 1500}}),
    ('POSTGRES_HANDLERS_COMMAND_CONTROL', {'/api/v1/example': {'POST': {'statement_timeout_ms': 1000}}}),
])
async def test_native_sql_settings_publish_without_conversion(admin_client, service_client, key, value):
    await publish_one(admin_client, key, value)
    assert (await current(service_client))['configs'][key] == value


@pytest.mark.parametrize('key,value', [
    ('POSTGRES_DEFAULT_COMMAND_CONTROL', {'network_timeout_ms': 0}),
    ('POSTGRES_DEFAULT_COMMAND_CONTROL', {'statement_timeout_ms': '1000'}),
    ('POSTGRES_DEFAULT_COMMAND_CONTROL', {'timeout': 1000}),
    ('POSTGRES_QUERIES_COMMAND_CONTROL', {'a_query': {'network_timeout_ms': -1}}),
    ('POSTGRES_QUERIES_COMMAND_CONTROL', {'a_query': {'unknown': True}}),
    ('POSTGRES_HANDLERS_COMMAND_CONTROL', {'/api/example': {'POST': {'statement_timeout_ms': 0}}}),
])
async def test_invalid_sql_settings_are_rejected_before_publication(admin_client, service_client, key, value):
    before = await current(service_client)
    response = await admin_client.post('/admin/configs/drafts/create', json={
        'config_name': key, 'description': 'Некорректный тайм-аут',
        'expected_config_version': 0, 'value': value})
    assert response.status == 400
    assert await current(service_client) == before
