"""Имена контрагентов и доступ проверяются с настоящим PostgreSQL."""

import asyncio
from uuid import uuid4
import pytest


async def test_create_replay_and_tenant_isolation(api, pgsql):
    identifier = str(uuid4())
    body = {'id': identifier, 'name': '  ООО Тестовый клиент  '}
    created = await api.post('/api/v1/counterparties/create', body, expected=201)
    assert created['counterparty'] == {'id': identifier, 'name': 'ООО Тестовый клиент'}
    assert await api.post('/api/v1/counterparties/create', body, expected=201) == created
    await api.post('/api/v1/counterparties/create', {**body, 'name': 'Другой'}, expected=409)
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT count(*) FROM business_audit WHERE entity_id = %s', (identifier,))
        assert cursor.fetchone()[0] == 1
    listed = await api.post('/api/v1/counterparties/list', {})
    assert listed['items'] == [created['counterparty']]
    assert (await api.post('/api/v1/counterparties/list', {}, user='other'))['items'] == []
    await api.post(
        '/api/v1/counterparties/create', body, roles=['counterparties.read'], expected=403
    )
    await api.post('/api/v1/counterparties/list', {}, roles=['shipments.read'], expected=403)


@pytest.mark.parametrize('name', ['', '   ', '\t\r\n', ' \t\r\n '])
async def test_blank_name(api, name):
    await api.post(
        '/api/v1/counterparties/create', {'id': str(uuid4()), 'name': name}, expected=400
    )


@pytest.mark.parametrize('name', ['Клиент', '  Клиент  ', '\tКлиент\r\n', 'Клиент с пробелами'])
async def test_trim_name(api, name):
    result = await api.post(
        '/api/v1/counterparties/create', {'id': str(uuid4()), 'name': name}, expected=201
    )
    assert result['counterparty']['name'] == name.strip()


async def test_concurrent_replay(api, pgsql):
    body = {'id': str(uuid4()), 'name': 'Клиент'}
    first, second = await asyncio.gather(
        api.post('/api/v1/counterparties/create', body, expected=201),
        api.post('/api/v1/counterparties/create', body, expected=201),
    )
    assert first == second
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT count(*) FROM business_audit WHERE entity_id = %s', (body['id'],))
        assert cursor.fetchone()[0] == 1
