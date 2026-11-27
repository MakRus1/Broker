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
        for position, port in enumerate(['Ningbo', 'Vostochny'], start=1):
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
            'booking': {'status': 'confirmed', 'number': reference},
        }}]
        shipment = (await api.post('/api/v1/shipments/create', creation(
            number=f'TEST-{index}', route_segments=route), expected=201))['shipment']
        visible = await api.post('/api/v1/shipments/get', {'shipment_id': shipment['id']}, roles=forwarder_permissions)
        assert visible['shipment']['route_segments'][0]['sea']['voyage'] == 'Carrier-043'
        assert 'confirmed_voyages' not in visible['shipment']
        with db.cursor() as cursor:
            cursor.execute("INSERT INTO voyage_assignments VALUES (%s,%s,%s,%s,1,2)",
                           (ORGANIZATION, shipment['id'], segment, voyage))
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
    assert detail['ports'] == ['Ningbo', 'Vostochny']
    assert detail['bookings'] == bookings
    assert (await api.post('/api/v1/voyages/list', {}, user='other'))['items'] == [
        {'id': other_voyage, 'number': 'L-043'}]
    await api.post('/api/v1/voyages/get', {'voyage_id': voyage}, user='other', expected=404)
    await api.post('/api/v1/voyages/get', {'voyage_id': voyage}, roles=['transportations.read'], expected=403)
