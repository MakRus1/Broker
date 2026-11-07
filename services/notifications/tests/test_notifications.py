import asyncio
import pytest
from conftest import ISSUER, ORGANIZATION, OTHER_ORGANIZATION

EVENT_PATH = '/api/internal/v1/events/publish'
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
    async def call(body, *, client='shipments-dispatcher', roles=None, envelope_changes=None):
        token = auth_key.token('service-account-' + client,
            roles=['notifications.publish_shipments'] if roles is None else roles,
            claims={'azp': client})
        envelope = {'id': body['organization_id'] + ':' + body['shipment_id'] + ':' + str(body['revision']),
            'type': 'shipment.route_changed', 'version': 1,
            'organization_id': body['organization_id'],
            'payload': {key: value for key, value in body.items() if key != 'organization_id'}}
        envelope.update(envelope_changes or {})
        return await service_client.post(EVENT_PATH, json=envelope,
            headers={'Authorization': 'Bearer ' + token})
    return call


async def test_duplicate_after_processing_and_read(api, publish):
    assert (await publish(event())).status == 200
    result = await api.post(LIST_PATH, {'organization_id': ORGANIZATION})
    assert len(result['items']) == 1
    item = result['items'][0]
    assert item['shipment_number'] == 'S-1042'
    assert item['event_type'] == 'shipment.route_changed'
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
    assert (await publish(event(), client='unknown-service')).status == 403
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


async def test_author_does_not_receive_own_route_change(api, publish, pgsql):
    own_event = event(actor_subject='author')
    assert (await publish(own_event)).status == 200
    assert (await publish(own_event)).status == 200
    assert (await api.post(LIST_PATH, {'organization_id': ORGANIZATION}))['items'] == []
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT count(*) FROM notification_events')
        assert cursor.fetchone()[0] == 1
    # Другой автор по-прежнему уведомляет назначенного логиста.
    assert (await publish(event(revision=3))).status == 200
    items = (await api.post(LIST_PATH, {'organization_id': ORGANIZATION}))['items']
    assert len(items) == 1 and items[0]['shipment_revision'] == 3


@pytest.mark.parametrize('changes', [
    {'type': 'unknown.event'},
    {'version': 2},
    {'payload': {}},
    {'payload': {key: value for key, value in event(revision=0).items() if key != 'organization_id'}},
    {'payload': {**{key: value for key, value in event().items() if key != 'organization_id'}, 'unexpected': True}},
    {'organization_id': 'not-a-uuid'},
    {'unexpected': True},
])
async def test_event_envelope_rejects_invalid_payload_and_unknown_types(publish, changes, pgsql):
    assert (await publish(event(), envelope_changes=changes)).status == 400
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT count(*) FROM notification_events')
        assert cursor.fetchone()[0] == 0


async def test_event_identity_cannot_be_reused_with_different_payload(publish):
    assert (await publish(event(), envelope_changes={'id': 'stable-event'})).status == 200
    assert (await publish(event(revision=3), envelope_changes={'id': 'stable-event'})).status == 409


async def test_typed_envelope_is_stored_without_duplicate_organization(publish, pgsql):
    assert (await publish(event())).status == 200
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT event_type, event_version, payload FROM notification_events')
        event_type, version, document = cursor.fetchone()
    assert event_type == 'shipment.route_changed' and version == 1
    assert document['organization_id'] == ORGANIZATION
    assert 'organization_id' not in document['payload']
    assert document['payload']['shipment_id'] == SHIPMENT


async def test_recipient_issuer_is_checked_before_transaction(publish, pgsql):
    response = await publish(event(recipient_issuer='https://other-issuer.invalid'))
    assert response.status == 400
    assert response.json()['error'] == 'invalid_recipient_issuer'
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT count(*) FROM notification_events')
        assert cursor.fetchone()[0] == 0


async def test_notification_write_failure_rolls_back_event(publish, api, pgsql):
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('ALTER TABLE notifications ADD CONSTRAINT test_reject_write CHECK (false) NOT VALID')
    try:
        assert (await publish(event())).status == 500
        with pgsql['db_1'].cursor() as cursor:
            cursor.execute('SELECT count(*) FROM notification_events')
            assert cursor.fetchone()[0] == 0
    finally:
        with pgsql['db_1'].cursor() as cursor:
            cursor.execute('ALTER TABLE notifications DROP CONSTRAINT test_reject_write')
    # Повтор того же ID после устранения ошибки должен создать уведомление.
    assert (await publish(event())).status == 200
    items = (await api.post(LIST_PATH, {'organization_id': ORGANIZATION}))['items']
    assert len(items) == 1

async def test_shipment_created_notifies_logist_and_skips_author(api,publish):
    changes={'type':'shipment.created','id':'shipment-created-1'}
    assert (await publish(event(revision=1),envelope_changes=changes)).status==200
    assert (await publish(event(revision=1),envelope_changes=changes)).status==200
    items=(await api.post(LIST_PATH,{'organization_id':ORGANIZATION}))['items']
    assert len(items)==1 and items[0]['event_type']=='shipment.created'
    own=event(shipment_id='bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb',actor_subject='author')
    assert (await publish(own,envelope_changes={'type':'shipment.created','id':'own-created'})).status==200
    assert len((await api.post(LIST_PATH,{'organization_id':ORGANIZATION}))['items'])==1
