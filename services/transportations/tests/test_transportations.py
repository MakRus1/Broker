"""Inbox-повторы и модель плеч с реальными транзакциями PostgreSQL."""

import asyncio
from uuid import uuid4
import pytest
from conftest import TENANT, OTHER_TENANT

PATH = '/api/internal/v1/transportations/ensure-created'


def command():
    return {
        'operation_id': str(uuid4()),
        'shipment_id': str(uuid4()),
        'item_id': str(uuid4()),
        'tenant_id': TENANT,
        'quantity': 5,
        'schema_version': 1,
        'container_type_code': '40HC',
        'container_ownership': 'COC',
    }


async def test_inbox_concurrent_replay(api):
    body = command()
    a, b = await asyncio.gather(api.post(PATH, body), api.post(PATH, body))
    assert a == b
    assert len(set(a['operation']['transportation_ids'])) == 5
    listed = await api.post('/api/v1/transportations/list', {'shipment_id': body['shipment_id']})
    assert len(listed['items']) == 5
    await api.post(PATH, {**body, 'quantity': 3}, expected=409)


async def test_tenant_and_service_role(api):
    body = command()
    await api.post(PATH, body, user='other', expected=403)
    await api.post(PATH, body, roles=['transportations.write'], expected=403)
    result = await api.post(PATH, body)
    await api.post(
        '/api/v1/transportations/get',
        {'transportation_id': result['operation']['transportation_ids'][0]},
        user='other',
        expected=404,
    )


def leg(mode='road'):
    return {
        'id': str(uuid4()),
        'mode': mode,
        'origin': {'name': 'Начало', 'country_code': 'RU'},
        'destination': {'name': 'Конец', 'country_code': 'RU'},
    }


async def test_route_actual_and_correction(api):
    created = await api.post(PATH, command())
    tid = created['operation']['transportation_ids'][0]
    legs = [leg(), leg('rail'), leg()]
    body = {'transportation_id': tid, 'expected_revision': 1, 'legs': legs}
    result = await api.post('/api/v1/transportation-routes/set', body)
    assert len(result['transportation']['legs']) == 3
    actual = {
        'transportation_id': tid,
        'leg_id': legs[0]['id'],
        'expected_revision': 2,
        'event_kind': 'departure',
        'occurred_at': '2026-10-20T09:00:00Z',
    }
    await api.post('/api/v1/transportation-actuals/record', actual)
    await api.post(
        '/api/v1/transportation-routes/set', {**body, 'expected_revision': 3}, expected=409
    )
    await api.post(
        '/api/v1/transportation-actuals/record', {**actual, 'expected_revision': 3}, expected=409
    )
    corrected = await api.post(
        '/api/v1/transportation-actuals/correct',
        {
            **actual,
            'expected_revision': 3,
            'reason': 'Уточнение времени',
            'occurred_at': '2026-10-20T10:00:00Z',
        },
    )
    assert corrected['transportation']['revision'] == 4


@pytest.mark.parametrize('mode', ['air', 'unknown'])
async def test_unsupported_transport(api, mode):
    created = await api.post(PATH, command())
    await api.post(
        '/api/v1/transportation-routes/set',
        {
            'transportation_id': created['operation']['transportation_ids'][0],
            'expected_revision': 1,
            'legs': [leg(mode)],
        },
        expected=400,
    )


async def test_duplicate_legs_and_invalid_time(api):
    created = await api.post(PATH, command())
    tid = created['operation']['transportation_ids'][0]
    first = leg()
    await api.post(
        '/api/v1/transportation-routes/set',
        {
            'transportation_id': tid,
            'expected_revision': 1,
            'legs': [first, {**first, 'id': first['id'].upper()}],
        },
        expected=400,
    )
    await api.post(
        '/api/v1/transportation-routes/set',
        {
            'transportation_id': tid,
            'expected_revision': 1,
            'legs': [
                {
                    **first,
                    'planned_departure_at': '2026-10-21T10:00:00Z',
                    'planned_arrival_at': '2026-10-21T09:00:00Z',
                }
            ],
        },
        expected=400,
    )
    unchanged = await api.post('/api/v1/transportations/get', {'transportation_id': tid})
    assert unchanged['transportation']['revision'] == 1
    assert unchanged['transportation']['legs'] == []
