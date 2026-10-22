"""Проверки количества, повторов, редакций и изоляции клиентов через HTTP и настоящую БД."""

import asyncio
from uuid import uuid4
import pytest


async def setup_item(api, quantity=5):
    shipment = (
        await api.post(
            '/api/v1/shipments/create',
            {
                'customer_id': str(uuid4()),
                'idempotency_key': str(uuid4()),
            },
            expected=201,
        )
    )['shipment']
    item = (
        await api.post(
            '/api/v1/shipment-items/add',
            {
                'shipment_id': shipment['id'],
                'expected_revision': shipment['revision'],
                'container_type_code': '40HC',
                'container_ownership': 'COC',
                'planned_quantity': quantity,
                'idempotency_key': str(uuid4()),
            },
            expected=201,
        )
    )['item']
    return shipment, item


async def test_reservation_replay_and_quantity(api):
    _, item = await setup_item(api)
    body = {
        'item_id': item['id'],
        'expected_revision': 1,
        'quantity': 3,
        'idempotency_key': str(uuid4()),
    }
    first = await api.post('/api/v1/transportation-operations/create', body, expected=202)
    assert await api.post('/api/v1/transportation-operations/create', body, expected=202) == first
    await api.post(
        '/api/v1/transportation-operations/create', {**body, 'quantity': 2}, expected=409
    )
    await api.post(
        '/api/v1/transportation-operations/create',
        {**body, 'idempotency_key': str(uuid4())},
        expected=409,
    )
    await api.post(
        '/api/v1/shipment-items/update',
        {'item_id': item['id'], 'expected_revision': 1, 'planned_quantity': 2},
        expected=409,
    )


async def test_concurrent_reservations(api):
    shipment, item = await setup_item(api)

    async def reserve():
        # Удобный клиент проверяет 202, поэтому конкурентный результат читаем отдельным способом.
        try:
            return await api.post(
                '/api/v1/transportation-operations/create',
                {
                    'item_id': item['id'],
                    'expected_revision': 1,
                    'quantity': 3,
                    'idempotency_key': str(uuid4()),
                },
                expected=202,
            )
        except AssertionError as error:
            assert 'Превышено' in str(error)
            return None

    results = await asyncio.gather(reserve(), reserve())
    assert sum(value is not None for value in results) == 1
    result = await api.post('/api/v1/shipment-items/list', {'shipment_id': shipment['id']})
    assert result['items'][0]['pending_quantity'] == 3
    assert result['items'][0]['available_quantity'] == 2


@pytest.mark.parametrize(
    'user,roles,status', [('other', None, 404), ('unknown', None, 403), ('author', [], 403)]
)
async def test_access(api, user, roles, status):
    shipment, _ = await setup_item(api)
    await api.post(
        '/api/v1/shipments/get',
        {'shipment_id': shipment['id']},
        user=user,
        roles=roles,
        expected=status,
    )


async def test_revision_and_idempotent_create(api):
    body = {'customer_id': str(uuid4()), 'reference': 'Первый', 'idempotency_key': str(uuid4())}
    first = await api.post('/api/v1/shipments/create', body, expected=201)
    assert await api.post('/api/v1/shipments/create', body, expected=201) == first
    shipment = first['shipment']
    update = {'shipment_id': shipment['id'], 'expected_revision': 1, 'reference': 'Новый'}
    changed = await api.post('/api/v1/shipments/update', update)
    assert changed['shipment']['revision'] == 2
    await api.post('/api/v1/shipments/update', update, expected=409)


@pytest.mark.parametrize('quantity', [0, -1, 101, '5', None])
async def test_invalid_quantity(api, quantity):
    _, item = await setup_item(api)
    await api.post(
        '/api/v1/transportation-operations/create',
        {
            'item_id': item['id'],
            'expected_revision': 1,
            'quantity': quantity,
            'idempotency_key': str(uuid4()),
        },
        expected=400,
    )


async def test_list_pagination_and_counters(api):
    shipment, item = await setup_item(api)
    await api.post(
        '/api/v1/transportation-operations/create',
        {
            'item_id': item['id'],
            'expected_revision': 1,
            'quantity': 2,
            'idempotency_key': str(uuid4()),
        },
        expected=202,
    )
    items = await api.post(
        '/api/v1/shipment-items/list', {'shipment_id': shipment['id'], 'limit': 1}
    )
    assert items['items'][0]['pending_quantity'] == 2
    assert items['items'][0]['available_quantity'] == 3
    empty = await api.post(
        '/api/v1/shipment-items/list',
        {
            'shipment_id': shipment['id'],
            'after': items['items'][0]['id'],
            'limit': 1,
        },
    )
    assert empty['items'] == []
    await setup_item(api)
    first = await api.post('/api/v1/shipments/list', {'limit': 1})
    second = await api.post(
        '/api/v1/shipments/list', {'limit': 1, 'after': first['items'][0]['id']}
    )
    assert len(second['items']) == 1
    assert second['items'][0]['id'] != first['items'][0]['id']


async def test_idempotent_replay_has_one_audit_entry(api, pgsql):
    body = {'customer_id': str(uuid4()), 'idempotency_key': str(uuid4())}
    first = await api.post('/api/v1/shipments/create', body, expected=201)
    assert await api.post('/api/v1/shipments/create', body, expected=201) == first
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute(
            'SELECT count(*) FROM business_audit WHERE entity_id = %s', (first['shipment']['id'],)
        )
        assert cursor.fetchone()[0] == 1


async def test_audit_failure_rolls_back_reservation(api, pgsql, service_client, auth_key):
    _, item = await setup_item(api)
    body = {
        'item_id': item['id'],
        'expected_revision': 1,
        'quantity': 3,
        'idempotency_key': str(uuid4()),
    }
    database = pgsql['db_1']
    # Ошибка последнего INSERT должна откатить outbox, идемпотентность и освободить блокировки.
    with database.cursor() as cursor:
        cursor.execute("""
            ALTER TABLE business_audit ADD CONSTRAINT test_reject_operation_audit
            CHECK (action <> 'create_operation') NOT VALID
        """)
    try:
        roles = ['shipments.read', 'shipments.write']
        token = auth_key.token(
            'author',
            roles=roles,
            claims={
                'aud': 'broker-logistics',
                'resource_access': {'broker-logistics': {'roles': roles}},
            },
        )
        response = await asyncio.wait_for(
            service_client.post(
                '/api/v1/transportation-operations/create',
                json=body,
                headers={'Authorization': 'Bearer ' + token},
            ),
            timeout=5,
        )
        assert response.status == 500
        with database.cursor() as cursor:
            cursor.execute(
                'SELECT count(*) FROM creation_operations WHERE item_id = %s::UUID', (item['id'],)
            )
            assert cursor.fetchone()[0] == 0
            cursor.execute(
                'SELECT count(*) FROM idempotent_results WHERE key = %s', (body['idempotency_key'],)
            )
            assert cursor.fetchone()[0] == 0
    finally:
        with database.cursor() as cursor:
            cursor.execute('ALTER TABLE business_audit DROP CONSTRAINT test_reject_operation_audit')
    # Тот же ключ и позиция доступны сразу после неудачной транзакции.
    result = await asyncio.wait_for(
        api.post('/api/v1/transportation-operations/create', body, expected=202),
        timeout=5,
    )
    assert result['operation']['quantity'] == 3
