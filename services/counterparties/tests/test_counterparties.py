"""Имена контрагентов и доступ проверяются с настоящим PostgreSQL."""

from uuid import uuid4


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


async def test_blank_name(api):
    await api.post(
        '/api/v1/counterparties/create', {'id': str(uuid4()), 'name': '   '}, expected=400
    )
