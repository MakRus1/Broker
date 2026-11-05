import asyncio
import pytest
from conftest import ISSUER, ORGANIZATION, OTHER_ORGANIZATION

EVENT_PATH = '/api/internal/v1/events/shipment-route-changed'
LIST_PATH = '/api/v1/notifications/list'
READ_PATH = '/api/v1/notifications/read'
SHIPMENT = 'aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa'


def event(**changes):
    return {
        'organization_id': ORGANIZATION,
        'shipment_id': SHIPMENT,
        'revision': 2,
        'shipment_number': 'S-1042',
        'recipient_issuer': ISSUER,
        'recipient_subject': 'author',
        'actor_subject': 'colleague',
        **changes,
    }


@pytest.fixture
def publish(service_client, auth_key):
    async def call(body, *, client='shipments-dispatcher', roles=None):
        token = auth_key.token('service-account-' + client,
            roles=['notifications.publish_shipments'] if roles is None else roles,
            claims={'azp': client})
        return await service_client.post(EVENT_PATH, json=body,
            headers={'Authorization': 'Bearer ' + token})
    return call


async def test_duplicate_after_processing_and_read(api, publish):
    assert (await publish(event())).status == 200
    result = await api.post(LIST_PATH, {'organization_id': ORGANIZATION})
    assert len(result['items']) == 1
    item = result['items'][0]
    assert item['shipment_number'] == 'S-1042'
    assert item['read'] is False
    assert await api.post(READ_PATH, {'organization_id': ORGANIZATION, 'ids': [item['id']]}) == {'updated': 1}
    # Событие уже обработано; поздний повтор не создаёт новое уведомление и не сбрасывает прочтение.
    assert (await publish(event())).status == 200
    items = (await api.post(LIST_PATH, {'organization_id': ORGANIZATION}))['items']
    assert len(items) == 1 and items[0]['id'] == item['id'] and items[0]['read']
    assert (await publish(event(shipment_number='different'))).status == 409


async def test_parallel_retries(api, publish):
    results = await asyncio.gather(*(publish(event()) for _ in range(8)))
    assert all(result.status == 200 for result in results)
    assert len((await api.post(LIST_PATH, {'organization_id': ORGANIZATION}))['items']) == 1


async def test_recipient_and_organization_isolation(api, publish):
    await publish(event())
    item = (await api.post(LIST_PATH, {'organization_id': ORGANIZATION}))['items'][0]
    assert (await api.post(LIST_PATH, {'organization_id': ORGANIZATION}, user='colleague'))['items'] == []
    assert await api.post(READ_PATH, {'organization_id': ORGANIZATION, 'ids': [item['id']]}, user='colleague') == {'updated': 0}
    await api.post(LIST_PATH, {'organization_id': ORGANIZATION}, user='other', expected=403)
    assert (await api.post(LIST_PATH, {'organization_id': OTHER_ORGANIZATION}, user='other'))['items'] == []
    await api.post(LIST_PATH, {'organization_id': ORGANIZATION}, user='outsider', expected=403)


async def test_trusted_producer_only(publish):
    assert (await publish(event(), client='broker-web')).status == 403
    assert (await publish(event(), roles=[])).status == 403
    assert (await publish(event(revision=0))).status == 400


async def test_pagination_is_scoped_and_stable(api, publish):
    for revision in range(1, 7):
        await publish(event(revision=revision))
    first = (await api.post(LIST_PATH, {'organization_id': ORGANIZATION, 'limit': 3}))['items']
    second = (await api.post(LIST_PATH, {'organization_id': ORGANIZATION, 'limit': 3, 'before': first[-1]['id']}))['items']
    assert len(first) == len(second) == 3
    assert len({item['id'] for item in first + second}) == 6
    assert [item['shipment_revision'] for item in first + second] == [6, 5, 4, 3, 2, 1]
    assert (await api.post(LIST_PATH, {'organization_id': ORGANIZATION, 'before': first[0]['id']}, user='colleague'))['items'] == []


async def test_live_permission_revocation(api, publish, mockserver):
    await publish(event())
    @mockserver.handler('/api/organizations/access')
    def revoked(request):
        return mockserver.make_response(status=403, json={'error': 'forbidden', 'message': 'Нет доступа'})
    await api.post(LIST_PATH, {'organization_id': ORGANIZATION}, expected=403)


async def test_dismiss_survives_replay(api, publish):
    await publish(event())
    item = (await api.post(LIST_PATH, {'organization_id': ORGANIZATION}))['items'][0]
    await api.post(READ_PATH, {'organization_id': ORGANIZATION, 'ids': [item['id']], 'dismiss': True})
    assert (await api.post(LIST_PATH, {'organization_id': ORGANIZATION}))['items'] == []
    assert (await publish(event())).status == 200
    assert (await api.post(LIST_PATH, {'organization_id': ORGANIZATION}))['items'] == []


async def test_event_without_assigned_logist_is_still_recorded(api, publish):
    assert (await publish(event(recipient_subject=None))).status == 200
    assert (await publish(event(recipient_subject=None))).status == 200
    assert (await api.post(LIST_PATH, {'organization_id': ORGANIZATION}))['items'] == []
    assert (await publish(event())).status == 409
