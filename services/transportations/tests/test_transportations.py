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
    numbers = [item['number'] for item in listed['items']]
    assert len(set(numbers)) == 5
    assert all(number.startswith('T-') for number in numbers)
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


@pytest.mark.parametrize(
    'departure,arrival,status',
    [
        ('2026-10-21T12:00:00+03:00', '2026-10-21T09:30:00Z', 200),
        ('2026-10-21T09:00:00.500Z', '2026-10-21T09:00:00.100Z', 400),
        ('2026-10-21t09:00:00z', '2026-10-21T09:00:00Z', 200),
    ],
)
async def test_route_timezones_and_fractions(api, departure, arrival, status):
    created = await api.post(PATH, command())
    await api.post(
        '/api/v1/transportation-routes/set',
        {
            'transportation_id': created['operation']['transportation_ids'][0],
            'expected_revision': 1,
            'legs': [{**leg(), 'planned_departure_at': departure, 'planned_arrival_at': arrival}],
        },
        expected=status,
    )


async def test_filtered_pagination_loads_all_legs(api):
    body = command()
    created = await api.post(PATH, body)
    tid = created['operation']['transportation_ids'][0]
    await api.post(
        '/api/v1/transportation-routes/set',
        {
            'transportation_id': tid,
            'expected_revision': 1,
            'legs': [leg('sea')],
        },
    )
    listed = await api.post(
        '/api/v1/transportations/list',
        {
            'shipment_id': body['shipment_id'],
            'item_id': body['item_id'],
            'limit': 100,
        },
    )
    assert len(listed['items']) == 5
    numbers = [item['number'] for item in listed['items']]
    assert len(set(numbers)) == 5
    assert all(number.startswith('T-') for number in numbers)
    selected = next(value for value in listed['items'] if value['id'] == tid)
    assert selected['revision'] == 2
    assert len(selected['legs']) == 1
    assert selected['legs'][0]['mode'] == 'sea'
    last = listed['items'][-1]['id']
    assert (await api.post('/api/v1/transportations/list', {'after': last}))['items'] == []


async def test_history_snapshots_pagination_and_access(api):
    created = await api.post(PATH, command())
    identifier = created['operation']['transportation_ids'][0]
    history_request = {'transportation_id': identifier}
    # UUID операции и перевозки принадлежат разным пространствам сущностей аудита.
    await api.post(PATH, {**command(), 'operation_id': identifier})
    assert (await api.post('/api/v1/transportations/history', history_request))['items'] == []
    route = leg('sea')
    await api.post(
        '/api/v1/transportation-routes/set',
        {'transportation_id': identifier, 'expected_revision': 1, 'legs': [route]},
    )
    actual = {
        'transportation_id': identifier,
        'leg_id': route['id'],
        'expected_revision': 2,
        'event_kind': 'departure',
        'occurred_at': '2026-10-21T10:00:00Z',
    }
    await api.post('/api/v1/transportation-actuals/record', actual)
    await api.post(
        '/api/v1/transportation-actuals/correct',
        {
            **actual,
            'expected_revision': 3,
            'occurred_at': '2026-10-21T11:00:00Z',
            'reason': 'Уточнено по данным терминала',
        },
    )
    latest = (await api.post('/api/v1/transportations/history', {**history_request, 'limit': 2}))[
        'items'
    ]
    assert [item['action'] for item in latest] == ['correct_actual', 'record_actual']
    assert [item['revision'] for item in latest] == [4, 3]
    assert latest[0]['actor_name'] == 'author'
    assert latest[0]['reason'] == 'Уточнено по данным терминала'
    assert latest[0]['event_kind'] == 'departure'
    assert latest[0]['legs'][0]['actual_departure_at'] == '2026-10-21T11:00:00Z'
    assert latest[1]['legs'][0]['actual_departure_at'] == '2026-10-21T10:00:00Z'
    earlier = (
        await api.post(
            '/api/v1/transportations/history', {**history_request, 'before': latest[-1]['id']}
        )
    )['items']
    assert len(earlier) == 1
    assert earlier[0]['action'] == 'set_route'
    await api.post('/api/v1/transportations/history', history_request, user='other', expected=404)
    await api.post(
        '/api/v1/transportations/history',
        history_request,
        roles=['transportations.write'],
        expected=403,
    )
    await api.post(
        '/api/v1/transportations/history', {'transportation_id': str(uuid4())}, expected=404
    )


async def test_history_read_permission_does_not_grant_writes(api):
    created = await api.post(PATH, command())
    identifier = created['operation']['transportation_ids'][0]
    result = await api.post(
        '/api/v1/transportations/history',
        {'transportation_id': identifier},
        roles=['transportations.read'],
    )
    assert result['items'] == []
    await api.post(
        '/api/v1/transportation-routes/set',
        {'transportation_id': identifier, 'expected_revision': 1, 'legs': [leg()]},
        roles=['transportations.read'],
        expected=403,
    )
