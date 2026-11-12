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


@pytest.mark.parametrize(
    'code,message',
    [
        ('shipment_revision_conflict', 'Поставка изменилась. Обновите перевозку'),
        ('route_changed', 'Участок больше не входит в маршрут'),
        ('another_conflict', 'Другая причина конфликта'),
    ],
)
async def test_route_change_rejected_before_local_write(
    api, shipment_routes, mockserver, code, message
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
        '/api/v1/transportation-routes/set',
        {
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
async def test_conflicting_save_does_not_register_route_usage_twice(
    api, shipment_routes, mockserver, path
):
    body = command()
    created = await api.post(PATH, body)
    identifier = created['operation']['transportation_ids'][0]
    stage = route_segment('road')
    shipment_routes.set(body['shipment_id'], [stage])
    both_loaded = asyncio.Event()
    loaded = 0

    @mockserver.json_handler('/api/v1/shipments/transportation-contexts')
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
    assert len(shipment_routes.registrations) == 1


@pytest.mark.parametrize('rollback', [False, True])
async def test_usage_waits_for_registered_save_to_commit_or_rollback(
    api, shipment_routes, mockserver, pgsql, rollback
):
    body = command()
    created = await api.post(PATH, body)
    identifier = created['operation']['transportation_ids'][0]
    stage = route_segment('road')
    shipment_routes.set(body['shipment_id'], [stage])
    registered = asyncio.Event()
    release_registration = asyncio.Event()

    if rollback:
        with pgsql['db_1'].cursor() as cursor:
            cursor.execute('''
                CREATE FUNCTION reject_route_write() RETURNS trigger LANGUAGE plpgsql AS $$
                BEGIN
                    RAISE EXCEPTION 'test rollback';
                END;
                $$;
                CREATE TRIGGER reject_route_write BEFORE INSERT ON route_segments
                FOR EACH ROW EXECUTE FUNCTION reject_route_write();
            ''')

    @mockserver.json_handler('/api/v1/shipments/route-segments/register-usage')
    async def register(request):
        registered.set()
        await asyncio.wait_for(release_registration.wait(), 5)
        return {'registered': True}

    save = asyncio.create_task(api.post(
        '/api/v1/transportation-actuals/record',
        {'transportation_id': identifier, 'expected_revision': 1,
         'route_segment_id': stage['id'], 'event_kind': 'departure',
         'occurred_at': '2026-10-20T09:00:00+00:00'},
        expected=500 if rollback else 200,
    ))
    usage = None
    try:
        await asyncio.wait_for(registered.wait(), 5)
        usage = asyncio.create_task(api.post(
            '/api/v1/transportation-routes/usage', {'shipment_id': body['shipment_id']},
            roles=['shipments.write', 'shipments.read', 'organizations.read_employees',
                   'counterparties.read'],
        ))
        with pytest.raises(asyncio.TimeoutError):
            await asyncio.wait_for(asyncio.shield(usage), 0.2)
    finally:
        release_registration.set()
        try:
            results = await asyncio.gather(save, *([usage] if usage is not None else []))
        finally:
            if rollback:
                with pgsql['db_1'].cursor() as cursor:
                    cursor.execute('DROP TRIGGER reject_route_write ON route_segments')
                    cursor.execute('DROP FUNCTION reject_route_write()')

    assert results[1]['segment_ids'] == ([] if rollback else [stage['id']])
