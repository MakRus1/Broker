import asyncio
from uuid import uuid4
import pytest
from conftest import ORGANIZATION


@pytest.mark.uservice_oneshot(config_hooks=['configure_dispatcher'])
async def test_route_event_retries_and_only_follows_route_changes(api, mockserver, pgsql):
    received = []

    @mockserver.json_handler('/service-token')
    def token(request):
        return {'access_token': 'notification-delivery-token'}

    @mockserver.json_handler('/api/internal/v1/events/publish')
    def accept(request):
        received.append(request.json)
        if len(received) == 1:
            return mockserver.make_response(status=503,
                json={'error': 'unavailable', 'message': 'Повторите позже'})
        return {'accepted': True}

    shipment = (await api.post('/api/v1/shipments/create', {
        'customer_id': str(uuid4()), 'idempotency_key': str(uuid4()),
        'logist': 'logist-author',
        'route_segments': [{'id': str(uuid4()), 'type': 'sea'}],
    }, expected=201))['shipment']
    unchanged = (await api.post('/api/v1/shipments/update', {
        'shipment_id': shipment['id'], 'expected_revision': shipment['revision'],
        'reference': 'Не меняем маршрут',
    }))['shipment']
    assert not received
    updated = (await api.post('/api/v1/shipments/update', {
        'shipment_id': shipment['id'], 'expected_revision': unchanged['revision'],
        'route_segments': [{'id': str(uuid4()), 'type': 'air'}],
    }))['shipment']
    for _ in range(160):
        if len(received) >= 2:
            break
        await asyncio.sleep(0.05)
    assert len(received) >= 2
    assert received[0] == received[1]
    assert received[0]['organization_id'] == ORGANIZATION
    assert received[0]['payload']['shipment_id'] == shipment['id']
    assert received[0]['payload']['revision'] == updated['revision']
    assert received[0]['payload']['recipient_subject'] == 'logist-author'
    assert received[0]['payload']['actor_subject'] == 'author'
    # Конфликт версии не должен создавать событие.
    await api.post('/api/v1/shipments/update', {
        'shipment_id': shipment['id'], 'expected_revision': unchanged['revision'],
        'route_segments': [{'id': str(uuid4()), 'type': 'sea'}],
    }, expected=409)
    for _ in range(60):
        with pgsql['db_1'].cursor() as cursor:
            cursor.execute("SELECT count(*) FROM outbox WHERE topic = 'notifications.publish'")
            if cursor.fetchone()[0] == 0:
                break
        await asyncio.sleep(0.05)
    else:
        pytest.fail('Принятое событие осталось в outbox')
    assert all(value['payload']['revision'] == updated['revision'] for value in received)
