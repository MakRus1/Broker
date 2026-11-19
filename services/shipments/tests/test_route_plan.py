"""План наземных участков сохраняется независимо от факта перевозки."""
from uuid import uuid4
import pytest


@pytest.mark.parametrize('mode', ['international_road', 'road', 'rail', 'international_rail', 'pickup'])
async def test_route_plan_roundtrip(api, mode):
    plan = {
        'origin_name': 'Роттердам', 'origin_country': 'NL', 'origin_address': 'Терминал отправления',
        'destination_name': 'Москва', 'destination_country': 'RU', 'destination_address': 'Станция / склад',
        'departure_date': '2030-01-10', 'arrival_date': '2030-01-15',
    }
    segment = {'id': str(uuid4()), 'type': mode, 'plan': plan}
    shipment = (await api.post('/api/v1/shipments/create', {
        'customer_id': str(uuid4()), 'idempotency_key': str(uuid4()), 'route_segments': [segment],
    }, expected=201))['shipment']
    assert shipment['route_segments'] == [segment]
    saved = (await api.post('/api/v1/shipments/get', {'shipment_id': shipment['id']}))['shipment']
    assert saved['route_segments'] == [segment]
    updated = (await api.post('/api/v1/shipments/update', {
        'shipment_id': shipment['id'], 'expected_revision': 1, 'reference': 'План сохранён',
    }))['shipment']
    assert updated['route_segments'] == [segment]


@pytest.mark.parametrize('plan', [
    {'departure_date': '1899-12-31'},
    {'departure_date': '2030-01-15', 'arrival_date': '2030-01-10'},
])
async def test_route_plan_dates(api, plan):
    await api.post('/api/v1/shipments/create', {
        'customer_id': str(uuid4()), 'idempotency_key': str(uuid4()),
        'route_segments': [{'id': str(uuid4()), 'type': 'road', 'plan': plan}],
    }, expected=400)
