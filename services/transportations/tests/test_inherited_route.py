"""Структура маршрута принадлежит поставке; поля и факты — конкретной перевозке."""

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
    sea, pickup = route_segment('sea'), route_segment('pickup')
    shipment_routes.set(body['shipment_id'], [sea, pickup], incoterms='FOB')
    edited = await api.post(
        '/api/v1/transportation-routes/set',
        {
            'transportation_id': first,
            'expected_revision': 1,
            'route_segments': [sea, pickup],
        },
    )
    assert edited['transportation']['revision'] == 2
    await api.post(
        '/api/v1/transportation-actuals/record',
        {
            'transportation_id': first,
            'expected_revision': 2,
            'route_segment_id': sea['id'],
            'event_kind': 'departure',
            'occurred_at': '2026-10-20T09:00:00Z',
        },
    )
    rail = route_segment('international_rail')
    shipment_routes.set(body['shipment_id'], [pickup, rail, sea], incoterms='CIF', revision=2)
    for identifier in [first, second]:
        actual = (await api.post('/api/v1/transportations/get', {'transportation_id': identifier}))[
            'transportation'
        ]
        assert [stage['id'] for stage in actual['route_segments']] == [
            pickup['id'],
            rail['id'],
            sea['id'],
        ]
        assert actual['incoterms'] == 'CIF'
        if identifier == first:
            assert actual['route_segments'][2]['actual_departure_at'] == '2026-10-20T09:00:00Z'
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
            'occurred_at': '2026-10-20T09:00:00Z',
            'reason': 'Нет исправляемого факта',
        },
        expected=409,
    )
    assert shipment_routes.registrations == []
