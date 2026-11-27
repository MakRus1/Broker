"""Два заказа с независимыми бронями на одном рейсе."""
from uuid import uuid4

from conftest import ORGANIZATION, OTHER_ORGANIZATION
from test_shipment_card import creation


async def test_voyage_from_both_workplaces(api, pgsql):
    db = pgsql['db_1']
    voyage, other_voyage = str(uuid4()), str(uuid4())
    with db.cursor() as cursor:
        cursor.execute(
            "INSERT INTO line_operations.voyages VALUES (%s, %s, 'L-043'), (%s, %s, 'L-043')",
            (ORGANIZATION, voyage, OTHER_ORGANIZATION, other_voyage),
        )
        for position, port in [(2, 'Ningbo'), (9, 'Vostochny')]:
            cursor.execute(
                "INSERT INTO line_operations.voyage_port_calls VALUES (%s,%s,%s,"
                "ROW(%s::uuid,'sea_port',%s,'',NULL)::transport_location_v1)",
                (ORGANIZATION, voyage, position, str(uuid4()), port),
            )
        cursor.execute(
            "INSERT INTO organization_shipment_rules "
            "(organization_id, number_template, reset_number_yearly, statuses, route_types, planned_containers) "
            "VALUES (%s, 'S-{seq}', false, ARRAY['Новая'], ARRAY['sea'], false) "
            "ON CONFLICT (organization_id) DO UPDATE SET planned_containers=false",
            (ORGANIZATION,),
        )

    assert (await api.post('/api/v1/voyages/get', {'voyage_id': voyage}))['bookings'] == []
    forwarder_permissions = ['shipments.read', 'counterparties.read', 'organizations.read_employees',
                             'transport_locations.read']
    await api.post('/api/v1/voyages/list', {}, roles=forwarder_permissions, expected=403)
    bookings = []
    for index in range(2):
        segment = str(uuid4())
        reference = f'BOOK-{index}'
        route = [{'id': segment, 'type': 'sea', 'sea': {
            'voyage': 'Carrier-043',
            'booking': {'status': 'placed', 'number': reference},
        }}]
        shipment = (await api.post('/api/v1/shipments/create', creation(
            number=f'TEST-{index}', route_segments=route), expected=201))['shipment']
        visible = await api.post('/api/v1/shipments/get', {'shipment_id': shipment['id']}, roles=forwarder_permissions)
        assert visible['shipment']['route_segments'][0]['sea']['voyage'] == 'Carrier-043'
        assert 'confirmed_voyages' not in visible['shipment']
        route[0]['sea']['booking']['status'] = 'confirmed'
        assignment = {'segment_id': segment, 'voyage_id': voyage, 'loading_call': 2, 'discharge_call': 9}
        confirm = {'shipment_id': shipment['id'], 'expected_revision': shipment['revision'],
                   'route_segments': route, 'voyage_assignment_changes': [assignment]}
        update = '/api/v1/shipments/update'
        await api.post(update, confirm, roles=[*forwarder_permissions, 'shipments.write'], expected=403)
        for invalid in [{'discharge_call': 2}, {'discharge_call': 10}, {'voyage_id': other_voyage}]:
            await api.post(update, {**confirm, 'voyage_assignment_changes': [{**assignment, **invalid}]}, expected=400)
        unchanged = await api.post('/api/v1/shipments/get', {'shipment_id': shipment['id']})
        assert unchanged['shipment']['route_segments'][0]['sea']['booking']['status'] == 'placed'
        updated = (await api.post(update, confirm))['shipment']
        await api.post(update, confirm, expected=409)
        linked = (await api.post('/api/v1/shipments/get', {'shipment_id': shipment['id']}))['shipment']
        external = (await api.post('/api/v1/shipments/get', {'shipment_id': shipment['id']}, roles=forwarder_permissions))['shipment']
        assert 'confirmed_voyages' not in external
        assert linked['confirmed_voyages'] == [{'segment_id': segment, 'voyage_id': voyage,
                                   'voyage_number': 'L-043', 'load_port': 'Ningbo',
                                   'discharge_port': 'Vostochny'}]
        bookings.append({'booking_number': reference, 'shipment_id': shipment['id'],
                         'shipment_number': shipment['number'], 'load_port': 'Ningbo',
                         'discharge_port': 'Vostochny'})

    assert (await api.post('/api/v1/voyages/list', {}))['items'] == [{'id': voyage, 'number': 'L-043'}]
    detail = await api.post('/api/v1/voyages/get', {'voyage_id': voyage})
    assert detail['ports'] == [{'name': 'Ningbo', 'position': 2}, {'name': 'Vostochny', 'position': 9}]
    assert detail['bookings'] == bookings
    assert (await api.post('/api/v1/voyages/list', {}, user='other'))['items'] == [
        {'id': other_voyage, 'number': 'L-043'}]
    await api.post('/api/v1/voyages/get', {'voyage_id': voyage}, user='other', expected=404)
    await api.post('/api/v1/voyages/get', {'voyage_id': voyage}, roles=['transportations.read'], expected=403)

    await api.post(update, {'shipment_id': updated['id'], 'expected_revision': updated['revision'],
                            'voyage_assignment_changes': [{'segment_id': segment}]})
    assert (await api.post('/api/v1/voyages/get', {'voyage_id': voyage}))['bookings'] == bookings[:1]
    first = (await api.post('/api/v1/shipments/get', {'shipment_id': bookings[0]['shipment_id']}))['shipment']
    first['route_segments'][0]['sea']['booking']['status'] = 'placed'
    changed = (await api.post(update, {'shipment_id': first['id'], 'expected_revision': first['revision'],
                                       'route_segments': first['route_segments']}))['shipment']
    changed['route_segments'][0]['sea']['booking']['status'] = 'confirmed'
    await api.post(update, {'shipment_id': first['id'], 'expected_revision': changed['revision'],
                            'route_segments': changed['route_segments']})
    assert (await api.post('/api/v1/voyages/get', {'voyage_id': voyage}))['bookings'] == []
