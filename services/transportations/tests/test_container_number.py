"""Номер реального контейнера вводят вручную; формат не ограничивает серверное сохранение."""

from uuid import uuid4
import pytest


def creation(**fields):
    return {
        'id': str(uuid4()),
        'shipment_id': str(uuid4()),
        'type': 'container',
        'hazardous': False,
        **fields,
    }


@pytest.mark.parametrize(
    'number', ['CSQU3054383', 'ABCD1234567', 'ожидается', '123', '  CSQU3054383  ']
)
async def test_container_number_roundtrip_and_clear(api, number):
    body = creation(container_number=number)
    created = (await api.post('/api/v1/transportations/create', body))['transportation']
    assert created['container_number'] == number.strip()
    assert 'number' not in created
    assert (await api.post('/api/v1/transportations/create', body))['transportation'] == created
    loaded = (await api.post('/api/v1/transportations/get', {'transportation_id': created['id']}))[
        'transportation'
    ]
    assert loaded == created
    listed = (await api.post('/api/v1/transportations/list', {'shipment_id': body['shipment_id']}))[
        'items'
    ]
    assert listed == [created]
    cleared = (
        await api.post(
            '/api/v1/transportations/update',
            {
                'transportation_id': created['id'],
                'expected_revision': 1,
                'type': 'container',
                'hazardous': False,
            },
        )
    )['transportation']
    assert 'container_number' not in cleared
    assert 'number' not in cleared


@pytest.mark.parametrize('fields', [{}, {'container_number': ''}, {'container_number': '  '}])
async def test_absent_number_is_not_generated(api, pgsql, fields):
    created = (await api.post('/api/v1/transportations/create', creation(**fields)))[
        'transportation'
    ]
    assert 'container_number' not in created
    assert 'number' not in created
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute(
            'SELECT container_number FROM transportations WHERE id = %s', (created['id'],)
        )
        assert cursor.fetchone()[0] is None


async def test_update_number_and_type(api):
    created = (await api.post('/api/v1/transportations/create', creation()))['transportation']
    updated = (
        await api.post(
            '/api/v1/transportations/update',
            {
                'transportation_id': created['id'],
                'expected_revision': 1,
                'type': 'container',
                'hazardous': False,
                'container_number': 'CSQU3054383',
            },
        )
    )['transportation']
    assert updated['container_number'] == 'CSQU3054383'
    await api.post(
        '/api/v1/transportations/update',
        {
            'transportation_id': created['id'],
            'expected_revision': 2,
            'type': 'air',
            'hazardous': False,
            'container_number': 'CSQU3054383',
        },
        expected=400,
    )
    switched = (
        await api.post(
            '/api/v1/transportations/update',
            {
                'transportation_id': created['id'],
                'expected_revision': 2,
                'type': 'air',
                'hazardous': False,
            },
        )
    )['transportation']
    assert 'container_number' not in switched


async def test_replay_before_number_change(api, pgsql):
    body = creation()
    created = (await api.post('/api/v1/transportations/create', body))['transportation']
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute(
            "UPDATE processed_commands SET result = result || '{\"number\": \"T-1\"}'::jsonb WHERE operation_id = %s",
            (body['id'],),
        )
    assert (await api.post('/api/v1/transportations/create', body))['transportation'] == created
