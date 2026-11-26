"""Inbox-повторы и модель плеч с реальными транзакциями PostgreSQL."""

import asyncio
import json
from uuid import uuid4
import pytest
from conftest import ORGANIZATION, OTHER_ORGANIZATION

PATH = '/api/internal/v1/transportations/ensure-created'


def command():
    return {
        'operation_id': str(uuid4()),
        'shipment_id': str(uuid4()),
        'item_id': str(uuid4()),
        'organization_id': ORGANIZATION,
        'quantity': 5,
        'schema_version': 1,
        'container_type_code': '40HC',
        'container_ownership': 'COC',
    }


async def test_inbox_concurrent_replay(api, pgsql):
    body = command()
    a, b = await asyncio.gather(api.post(PATH, body), api.post(PATH, body))
    assert a == b
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute(
            "UPDATE processed_commands SET payload = jsonb_set(payload, '{shipment_id}', to_jsonb(%s::text)) WHERE operation_id = %s",
            (body['shipment_id'].upper(), body['operation_id']),
        )
    assert await api.post(PATH, body) == a
    assert len(set(a['operation']['transportation_ids'])) == 5
    listed = await api.post(
        '/api/v1/transportations/list',
        {'filters': [{'field': 'shipment_id', 'values': [body['shipment_id']]}]},
    )
    assert len(listed['items']) == 5
    assert {item['id'] for item in listed['items']} == set(a['operation']['transportation_ids'])
    for item in listed['items']:
        for field in ('shipment_id', 'item_id', 'operation_id'):
            assert item[field] == body[field]
    assert all(
        'number' not in item and 'container_number' not in item for item in listed['items']
    )
    await api.post(PATH, {**body, 'quantity': 3}, expected=409)


async def test_organization_and_service_role(api):
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


def route_segment(mode='road'):
    return {
        'id': str(uuid4()),
        'mode': mode,
        'origin': {'name': 'Начало', 'country_code': 'RU'},
        'destination': {'name': 'Конец', 'country_code': 'RU'},
    }


async def test_route_actual_and_correction(api, shipment_routes):
    body_command = command()
    created = await api.post(PATH, body_command)
    tid = created['operation']['transportation_ids'][0]
    route_segments = [route_segment(), route_segment('rail'), route_segment()]
    shipment_routes.set(body_command['shipment_id'], route_segments)
    body = {'transportation_id': tid, 'expected_revision': 1, 'route_segments': route_segments}
    result = await api.post('/api/v1/transportation-routes/set', body)
    assert len(result['transportation']['route_segments']) == 3
    actual = {
        'transportation_id': tid,
        'route_segment_id': route_segments[0]['id'],
        'expected_revision': 2,
        'event_kind': 'departure',
        'occurred_at': '2026-10-20T09:00:00+00:00',
    }
    await api.post('/api/v1/transportation-actuals/record', actual)
    await api.post(
        '/api/v1/transportation-actuals/record', {**actual, 'expected_revision': 3}, expected=409
    )
    corrected = await api.post(
        '/api/v1/transportation-actuals/correct',
        {
            **actual,
            'expected_revision': 3,
            'reason': 'Уточнение времени',
            'occurred_at': '2026-10-20T10:00:00+00:00',
        },
    )
    assert corrected['transportation']['revision'] == 4


@pytest.mark.parametrize('mode', ['unknown', 'space'])
async def test_unsupported_transport(api, mode):
    created = await api.post(PATH, command())
    await api.post(
        '/api/v1/transportation-routes/set',
        {
            'transportation_id': created['operation']['transportation_ids'][0],
            'expected_revision': 1,
            'route_segments': [route_segment(mode)],
        },
        expected=400,
    )


async def test_duplicate_legs_and_invalid_time(api):
    body_command = command()
    created = await api.post(PATH, body_command)
    tid = created['operation']['transportation_ids'][0]
    first = route_segment()
    await api.post(
        '/api/v1/transportation-routes/set',
        {
            'transportation_id': tid,
            'expected_revision': 1,
            'route_segments': [first, {**first, 'id': first['id'].upper()}],
        },
        expected=400,
    )
    await api.post(
        '/api/v1/transportation-routes/set',
        {
            'transportation_id': tid,
            'expected_revision': 1,
            'route_segments': [
                {
                    **first,
                    'planned_departure_at': '2026-10-21T10:00:00+00:00',
                    'planned_arrival_at': '2026-10-21T09:00:00+00:00',
                }
            ],
        },
        expected=400,
    )
    unchanged = await api.post('/api/v1/transportations/get', {'transportation_id': tid})
    assert unchanged['transportation']['revision'] == 1
    assert unchanged['transportation']['route_segments'] == []


@pytest.mark.parametrize(
    'departure,arrival,status',
    [
        ('2026-10-21T12:00:00+03:00', '2026-10-21T09:30:00+00:00', 200),
        ('2026-10-21T09:00:00.500+00:00', '2026-10-21T09:00:00.100+00:00', 400),
        ('2026-10-21t09:00:00z', '2026-10-21T09:00:00+00:00', 200),
    ],
)
async def test_route_timezones_and_fractions(api, shipment_routes, departure, arrival, status):
    body = command()
    created = await api.post(PATH, body)
    stage = route_segment()
    shipment_routes.set(body['shipment_id'], [stage])
    await api.post(
        '/api/v1/transportation-routes/set',
        {
            'transportation_id': created['operation']['transportation_ids'][0],
            'expected_revision': 1,
            'route_segments': [
                {**stage, 'planned_departure_at': departure, 'planned_arrival_at': arrival}
            ],
        },
        expected=status,
    )


async def test_filtered_pagination_loads_all_route_segments(api, shipment_routes):
    body = command()
    created = await api.post(PATH, body)
    tid = created['operation']['transportation_ids'][0]
    stage = route_segment('sea')
    shipment_routes.set(body['shipment_id'], [stage])
    await api.post(
        '/api/v1/transportation-routes/set',
        {
            'transportation_id': tid,
            'expected_revision': 1,
            'route_segments': [stage],
        },
    )
    listed = await api.post(
        '/api/v1/transportations/list',
        {
            'limit': 100,
            'filters': [
                {'field': 'shipment_id', 'values': [body['shipment_id']]},
                {'field': 'item_id', 'values': [body['item_id']]},
            ],
        },
    )
    assert len(listed['items']) == 5
    assert all(
        'number' not in item and 'container_number' not in item for item in listed['items']
    )
    selected = next(value for value in listed['items'] if value['id'] == tid)
    assert selected['revision'] == 2
    assert len(selected['route_segments']) == 1
    assert selected['route_segments'][0]['mode'] == 'sea'
    last = listed['items'][-1]['id']
    assert (await api.post('/api/v1/transportations/list', {'after': last}))['items'] == []


async def test_history_snapshots_pagination_and_access(api, shipment_routes):
    source_command = command()
    created = await api.post(PATH, source_command)
    identifier = created['operation']['transportation_ids'][0]
    history_request = {'transportation_id': identifier}
    # UUID операции и перевозки принадлежат разным пространствам сущностей аудита.
    await api.post(PATH, {**command(), 'operation_id': identifier})
    assert (await api.post('/api/v1/transportations/history', history_request))['items'] == []
    route = route_segment('pickup')
    shipment_routes.set(source_command['shipment_id'], [route])
    await api.post(
        '/api/v1/transportation-routes/set',
        {'transportation_id': identifier, 'expected_revision': 1, 'route_segments': [route]},
    )
    actual = {
        'transportation_id': identifier,
        'route_segment_id': route['id'],
        'expected_revision': 2,
        'event_kind': 'departure',
        'occurred_at': '2026-10-21T10:00:00+00:00',
    }
    await api.post('/api/v1/transportation-actuals/record', actual)
    await api.post(
        '/api/v1/transportation-actuals/correct',
        {
            **actual,
            'expected_revision': 3,
            'occurred_at': '2026-10-21T11:00:00+00:00',
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
    assert latest[0]['route_segments'][0]['actual_departure_at'] == '2026-10-21T11:00:00+00:00'
    assert latest[1]['route_segments'][0]['actual_departure_at'] == '2026-10-21T10:00:00+00:00'
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
        roles=['transportations.read', 'transport_locations.read'],
    )
    assert result['items'] == []
    await api.post(
        '/api/v1/transportation-routes/set',
        {
            'transportation_id': identifier,
            'expected_revision': 1,
            'route_segments': [route_segment()],
        },
        roles=['transportations.read', 'transport_locations.read'],
        expected=403,
    )


async def test_actual_container_iso_code_roundtrip_and_validation(api):
    create = '/api/v1/transportations/create'
    update = '/api/v1/transportations/update'
    body = {
        'id': str(uuid4()), 'shipment_id': str(uuid4()), 'type': 'container',
        'hazardous': False, 'container_type_code': '40HC', 'container_iso_code': '45G1',
    }
    invalid = await api.post(create, {**body, 'container_iso_code': '22G1'}, expected=400)
    assert invalid['error'] == 'invalid_container_iso_code'
    invalid = await api.post(create, {**body, 'container_type_code': 'OTHER'}, expected=400)
    assert invalid['error'] == 'invalid_container_type'
    created = (await api.post(create, body))['transportation']
    assert created['container_iso_code'] == '45G1'
    assert (await api.post(create, body))['transportation'] == created

    change = {
        'transportation_id': body['id'], 'expected_revision': 1,
        'type': 'container', 'hazardous': True, 'container_type_code': '40HC',
    }
    saved = (await api.post(update, change))['transportation']
    assert saved['container_iso_code'] == '45G1'
    saved = (await api.post(
        update, {**change, 'expected_revision': 2, 'container_iso_code': None},
    ))['transportation']
    assert 'container_iso_code' not in saved
    listed = await api.post('/api/v1/transportations/list', {
        'filters': [{'field': 'shipment_id', 'values': [body['shipment_id']]}],
    })
    assert 'container_iso_code' not in listed['items'][0]


async def test_existing_plan_group_outside_catalog_can_be_kept(api):
    plan = {**command(), 'container_type_code': 'LEGACY', 'quantity': 1}
    created = await api.post(PATH, plan)
    item_id = created['operation']['transportation_ids'][0]
    saved = (await api.post('/api/v1/transportations/update', {
        'transportation_id': item_id, 'expected_revision': 1, 'type': 'container',
        'hazardous': True, 'container_type_code': 'LEGACY', 'container_ownership': 'COC',
    }))['transportation']
    assert saved['container_type_code'] == 'LEGACY'
    assert 'container_iso_code' not in saved


async def test_shipment_route_progress_counts_only_current_legs(api, pgsql):
    plan = {**command(), 'quantity': 3}
    created = await api.post(PATH, plan)
    first, second, third = created['operation']['transportation_ids']
    road, sea = str(uuid4()), str(uuid4())
    with pgsql['db_1'].cursor() as cursor:
        for transportation, stages in [
            (first, [{'id': road, 'mode': 'international_road',
                      'actual_departure_date': '2026-11-25',
                      'international_road': {'unloading_date': '2026-11-26'}}]),
            (second, [{'id': road, 'mode': 'international_road',
                       'actual_departure_at': '2026-11-25T08:00:00Z'},
                      {'id': sea, 'mode': 'sea', 'actual_arrival_at': '2026-11-26T08:00:00Z'}]),
            (third, [{'id': road, 'mode': 'international_road',
                      'international_road': {'departure_date': '2026-11-25'}}]),
        ]:
            cursor.execute('UPDATE transportations SET route_segments = %s::jsonb WHERE id = %s',
                           (json.dumps(stages), transportation))
    path = '/api/v1/transportations/shipment-route-progress'
    query = {'shipment_id': plan['shipment_id']}
    result = await api.post(path, query, roles=['transportations.read', 'transport_locations.read'])
    assert result == {'total': 3, 'current': [{'route_segment_id': road, 'quantity': 1}]}
    assert await api.post(path, query, user='other', roles=['transportations.read', 'transport_locations.read']) == {
        'total': 0, 'current': [],
    }
    await api.post(path, query, roles=['shipments.read'], expected=403)
