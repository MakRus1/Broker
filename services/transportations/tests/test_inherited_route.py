"""Структура маршрута принадлежит поставке; поля и факты — конкретной перевозке."""

import asyncio
from uuid import uuid4
import pytest
from test_transportations import command, route_segment, PATH


@pytest.mark.parametrize(
    'kind', ['consolidated', 'container', 'air', 'oversized', 'rail', 'vessel']
)
async def test_manual_creation_and_replay(api, shipment_routes, kind):
    shipment = str(uuid4())
    stages = [route_segment('sea'), route_segment('pickup')]
    shipment_routes.set(shipment, stages, incoterms='FOB', loading='FCL', carriage_terms='FIFO')
    body = {'id': str(uuid4()), 'shipment_id': shipment, 'type': kind, 'hazardous': True}
    created = (await api.post('/api/v1/transportations/create', body))['transportation']
    assert created['type'] == kind
    assert created['hazardous'] is True
    assert created['incoterms'] == 'FOB'
    assert created['loading'] == 'FCL'
    assert created['carriage_terms'] == 'FIFO'
    assert 'item_id' not in created
    assert 'operation_id' not in created
    assert 'container_type_code' not in created
    assert [stage['id'] for stage in created['route_segments']] == [stage['id'] for stage in stages]
    assert (await api.post('/api/v1/transportations/create', body))['transportation'] == created
    await api.post('/api/v1/transportations/create', {**body, 'hazardous': False}, expected=409)
    await api.post(
        '/api/v1/transportations/get',
        {'transportation_id': created['id']},
        user='other',
        expected=404,
    )


async def test_live_route_reorder_and_terms_keep_local_data(api, shipment_routes):
    body = command()
    created = await api.post(PATH, body)
    first, second = created['operation']['transportation_ids'][:2]
    road, pickup = route_segment('international_road'), route_segment('pickup')
    shipment_routes.set(body['shipment_id'], [road, pickup], incoterms='FOB')
    edited = await api.post(
        '/api/v1/transportation-routes/set',
        {
            'transportation_id': first,
            'expected_revision': 1,
            'route_segments': [road, pickup],
        },
    )
    assert edited['transportation']['revision'] == 2
    await api.post(
        '/api/v1/transportation-actuals/record',
        {
            'transportation_id': first,
            'expected_revision': 2,
            'route_segment_id': road['id'],
            'event_kind': 'departure',
            'occurred_at': '2026-10-20T09:00:00+00:00',
        },
    )
    rail = route_segment('international_rail')
    shipment_routes.set(body['shipment_id'], [pickup, rail, road], incoterms='CIF', revision=2)
    for identifier in [first, second]:
        actual = (await api.post('/api/v1/transportations/get', {'transportation_id': identifier}))[
            'transportation'
        ]
        assert [stage['id'] for stage in actual['route_segments']] == [
            pickup['id'],
            rail['id'],
            road['id'],
        ]
        assert actual['incoterms'] == 'CIF'
        if identifier == first:
            assert actual['route_segments'][2]['actual_departure_at'] == '2026-10-20T09:00:00+00:00'
            assert actual['route_segments'][0]['origin']['name'] == 'Начало'
        else:
            assert 'origin' not in actual['route_segments'][0]
    assert shipment_routes.registrations


async def test_structure_cannot_be_changed_in_transportation(api, shipment_routes):
    body = command()
    created = await api.post(PATH, body)
    stage = route_segment('sea')
    shipment_routes.set(body['shipment_id'], [stage])
    await api.post(
        '/api/v1/transportation-routes/set',
        {
            'transportation_id': created['operation']['transportation_ids'][0],
            'expected_revision': 1,
            'route_segments': [{**stage, 'mode': 'road'}],
        },
        expected=409,
    )
    assert shipment_routes.registrations == []


async def test_update_details_keeps_facts_and_rejects_old_revision(api, shipment_routes):
    body = {
        'id': str(uuid4()),
        'shipment_id': str(uuid4()),
        'type': 'container',
        'hazardous': False,
        'container_type_code': '40HC',
        'container_ownership': 'SOC',
    }
    created = (await api.post('/api/v1/transportations/create', body))['transportation']
    update = {
        'transportation_id': created['id'],
        'expected_revision': 1,
        'type': 'air',
        'hazardous': True,
    }
    changed = (await api.post('/api/v1/transportations/update', update))['transportation']
    assert changed['revision'] == 2
    assert changed['type'] == 'air'
    assert changed['hazardous'] is True
    assert 'container_type_code' not in changed
    assert 'container_ownership' not in changed
    await api.post('/api/v1/transportations/update', update, expected=409)
    await api.post(
        '/api/v1/transportations/update',
        {**update, 'expected_revision': 2, 'container_type_code': '40HC'},
        expected=400,
    )


async def test_update_details_and_route_in_one_revision(api, shipment_routes):
    shipment = str(uuid4())
    stage = route_segment('road')
    shipment_routes.set(shipment, [stage])
    created = (await api.post('/api/v1/transportations/create', {
        'id': str(uuid4()), 'shipment_id': shipment, 'type': 'container', 'hazardous': False,
    }))['transportation']
    actual = (await api.post('/api/v1/transportation-actuals/record', {
        'transportation_id': created['id'], 'expected_revision': 1,
        'route_segment_id': stage['id'], 'event_kind': 'departure',
        'occurred_at': '2026-10-20T09:00:00+00:00',
    }))['transportation']
    update = {
        'transportation_id': created['id'], 'expected_revision': actual['revision'],
        'type': 'container', 'hazardous': True, 'container_number': 'CSQU3054383',
        'route_segments': [stage],
    }
    saved = (await api.post('/api/v1/transportations/update', update))['transportation']
    assert saved['revision'] == actual['revision'] + 1
    assert saved['container_number'] == 'CSQU3054383'
    assert saved['hazardous'] is True
    assert saved['route_segments'][0]['origin'] == stage['origin']
    assert saved['route_segments'][0]['actual_departure_at'] == '2026-10-20T09:00:00+00:00'
    await api.post('/api/v1/transportations/update', update, expected=409)
    loaded = (await api.post('/api/v1/transportations/get', {'transportation_id': created['id']}))['transportation']
    assert loaded == saved


@pytest.mark.parametrize(
    'code,message',
    [
        ('shipment_revision_conflict', 'Поставка изменилась. Обновите перевозку'),
        ('route_changed', 'Участок больше не входит в маршрут'),
        ('another_conflict', 'Другая причина конфликта'),
    ],
)
@pytest.mark.parametrize('combined', [False, True])
async def test_route_change_rejected_before_local_write(
    api, shipment_routes, mockserver, code, message, combined
):
    body = command()
    operation = await api.post(PATH, body)
    identifier = operation['operation']['transportation_ids'][0]
    stage = route_segment()
    shipment_routes.set(body['shipment_id'], [stage])

    @mockserver.json_handler('/api/v1/shipments/route-segments/register-usage')
    def changed(request):
        return mockserver.make_response(status=409, json={'error': code, 'message': message})

    conflict = await api.post(
        '/api/v1/transportations/update' if combined else '/api/v1/transportation-routes/set',
        {
            **({'type': 'container', 'hazardous': True, 'container_number': 'CSQU3054383'} if combined else {}),
            'transportation_id': identifier,
            'expected_revision': 1,
            'route_segments': [stage],
        },
        expected=409,
    )
    assert conflict['error'] == code
    assert conflict['message'] == message
    loaded = (await api.post('/api/v1/transportations/get', {'transportation_id': identifier}))[
        'transportation'
    ]
    assert loaded['revision'] == 1
    assert loaded['hazardous'] is False
    assert 'container_number' not in loaded
    assert 'origin' not in loaded['route_segments'][0]


async def test_invalid_actual_does_not_protect_segment(api, shipment_routes):
    body = command()
    operation = await api.post(PATH, body)
    identifier = operation['operation']['transportation_ids'][0]
    stage = route_segment()
    shipment_routes.set(body['shipment_id'], [stage])
    await api.post(
        '/api/v1/transportation-actuals/correct',
        {
            'transportation_id': identifier,
            'expected_revision': 1,
            'route_segment_id': stage['id'],
            'event_kind': 'departure',
            'occurred_at': '2026-10-20T09:00:00+00:00',
            'reason': 'Нет исправляемого факта',
        },
        expected=409,
    )
    assert shipment_routes.registrations == []


@pytest.mark.parametrize('path', [
    '/api/v1/transportation-routes/set',
    '/api/v1/transportation-actuals/record',
])
async def test_competing_route_writes_commit_one_revision(
    api, shipment_routes, mockserver, path
):
    body = command()
    created = await api.post(PATH, body)
    identifier = created['operation']['transportation_ids'][0]
    stage = route_segment('road')
    shipment_routes.set(body['shipment_id'], [stage])
    both_loaded = asyncio.Event()
    loaded = 0

    @mockserver.json_handler('/api/v1/shipments/route-contexts')
    async def load(request):
        nonlocal loaded
        loaded += 1
        if loaded == 2:
            both_loaded.set()
        await asyncio.wait_for(both_loaded.wait(), 5)
        return {'items': [{
            'shipment_id': body['shipment_id'], 'revision': 1,
            'route_segments': [{'id': stage['id'], 'type': stage['mode']}],
        }]}

    change = {'transportation_id': identifier, 'expected_revision': 1}
    if path.endswith('/set'):
        change['route_segments'] = [stage]
    else:
        change.update(route_segment_id=stage['id'], event_kind='departure',
                      occurred_at='2026-10-20T09:00:00+00:00')

    responses = await asyncio.gather(*(
        api.post(path, change, expected=(200, 409)) for _ in range(2)
    ))
    assert sum('transportation' in response for response in responses) == 1
    assert 1 <= len(shipment_routes.registrations) <= 2
    assert all(entry['segment_ids'] == [stage['id']] for entry in shipment_routes.registrations)
    current = (await api.post('/api/v1/transportations/get', {'transportation_id': identifier}))['transportation']
    assert current['revision'] == 2


@pytest.mark.parametrize('failure', ['concurrent_usage_check', 'local_rollback', 'remote_failure'])
@pytest.mark.parametrize('path', [
    '/api/v1/transportation-routes/set',
    '/api/v1/transportation-actuals/record',
    '/api/v1/transportations/create',
    '/api/v1/transportations/update',
])
async def test_failed_route_write_leaves_no_usage_and_can_be_retried(
    api, shipment_routes, mockserver, pgsql, failure, path
):
    body = command()
    stage = route_segment('road')
    shipment_routes.set(body['shipment_id'], [stage])
    registered = asyncio.Event()
    release_registration = asyncio.Event()

    if failure == 'local_rollback':
        with pgsql['db_1'].cursor() as cursor:
            cursor.execute('''
                CREATE FUNCTION reject_route_write() RETURNS trigger LANGUAGE plpgsql AS $$
                BEGIN
                    RAISE EXCEPTION 'test rollback';
                END;
                $$;
                CREATE TRIGGER reject_route_write BEFORE UPDATE OF route_segments ON transportations
                FOR EACH ROW EXECUTE FUNCTION reject_route_write();
            ''')

    attempts = 0

    @mockserver.handler('/api/v1/shipments/route-segments/register-usage')
    async def register(request):
        nonlocal attempts
        attempts += 1
        if attempts == 1:
            registered.set()
            await asyncio.wait_for(release_registration.wait(), 5)
            if failure == 'remote_failure':
                return mockserver.make_response(status=503, json={'error': 'unavailable', 'message': 'retry'})
        return mockserver.make_response(status=200, json={'registered': True})

    if path.endswith('/create'):
        change = {
            'id': str(uuid4()), 'shipment_id': body['shipment_id'],
            'type': 'container', 'hazardous': False, 'route_segments': [stage],
        }
    else:
        created = await api.post(PATH, body)
        identifier = created['operation']['transportation_ids'][0]
        change = {'transportation_id': identifier, 'expected_revision': 1}
        if path.endswith('/update'):
            change.update(type='container', hazardous=True, route_segments=[stage])
        elif path.endswith('/set'):
            change['route_segments'] = [stage]
        else:
            change.update(route_segment_id=stage['id'], event_kind='departure',
                          occurred_at='2026-10-20T09:00:00+00:00')

    status = {'concurrent_usage_check': 409, 'local_rollback': 500, 'remote_failure': 503}[failure]
    save = asyncio.create_task(api.post(path, change, expected=status))

    async def read_usage():
        return (await api.post('/api/v1/transportation-routes/usage', {'shipment_id': body['shipment_id']},
            roles=['shipments.write', 'shipments.read', 'organizations.read_employees',
                   'counterparties.read', 'transport_locations.read']))['segment_ids']

    try:
        await asyncio.wait_for(registered.wait(), 5)
        if failure == 'concurrent_usage_check':
            # Shipments получает снимок, пока HTTP-регистрация ещё не ответила.
            assert await asyncio.wait_for(read_usage(), 2) == []
    finally:
        release_registration.set()
        try:
            response = await save
        finally:
            if failure == 'local_rollback':
                with pgsql['db_1'].cursor() as cursor:
                    cursor.execute('DROP TRIGGER reject_route_write ON transportations')
                    cursor.execute('DROP FUNCTION reject_route_write()')

    if failure == 'concurrent_usage_check':
        assert response['error'] == 'route_usage_changed'
    assert await read_usage() == []
    await api.post(path, change)
    assert await read_usage() == [stage['id']]


async def test_full_route_write_keeps_historical_documents(api, shipment_routes, pgsql):
    import json
    shipment = str(uuid4())
    stage = route_segment('road')
    shipment_routes.set(shipment, [stage])
    created = (await api.post('/api/v1/transportations/create', {
        'id': str(uuid4()), 'shipment_id': shipment, 'type': 'container', 'hazardous': False,
    }))['transportation']
    historical = {
        'id': str(uuid4()), 'mode': 'sea', 'inherited': False,
        'contractor_id': str(uuid4()), 'sea': {'vessel': 'Историческое судно'},
        'plan': {'origin_name': 'Исторический порт'},
    }
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('UPDATE transportations SET route_segments=%s::jsonb WHERE id=%s',
                       (json.dumps([historical]), created['id']))
    saved = (await api.post('/api/v1/transportations/update', {
        'transportation_id': created['id'], 'expected_revision': 1,
        'type': 'container', 'hazardous': True, 'route_segments': [stage],
    }))['transportation']
    assert saved['route_segments'][-1] == historical
    await api.post('/api/v1/transportation-actuals/record', {
        'transportation_id': created['id'], 'expected_revision': 2,
        'route_segment_id': stage['id'], 'event_kind': 'departure',
        'occurred_at': '2026-10-20T09:00:00+00:00',
    })
    loaded = (await api.post('/api/v1/transportations/get', {'transportation_id': created['id']}))['transportation']
    assert loaded['route_segments'][-1] == historical
