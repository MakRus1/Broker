import copy
import uuid
import pytest

ORG = '11111111-1111-4111-8111-111111111111'
CUSTOMER = '33333333-3333-4333-8333-333333333333'
STAGE = '44444444-4444-4444-8444-444444444444'
SHIPMENT = '55555555-5555-4555-8555-555555555555'

@pytest.fixture(autouse=True)
def references(mockserver):
    @mockserver.json_handler('/api/v1/counterparties/get')
    def party(request):
        return {'counterparty': {'id': request.json['id'], 'name': 'Клиент',
            'revision': 1, 'archived': False, 'roles': [], 'own_company': False, 'contacts': [], 'legal_name':None, 'country_code':'RU', 'tax_id':None, 'registration_id':None, 'legal_address':None}}
    @mockserver.json_handler('/api/organizations/employees/list')
    def employee(request):
        return {'items': [{'subject': 'author', 'display_name': 'Менеджер'}]}

async def settings(api, stages=None, revision=0):
    return await api.post('/api/admin/sales/settings/update', {
        'organization_id': ORG, 'expected_revision': revision,
        'stages': stages or [{'id': STAGE, 'name': 'Знакомство', 'enabled': True}]})

def card():
    return {'id': str(uuid.uuid4()), 'customer_id': CUSTOMER, 'manager_subject': 'author',
        'stage_id': STAGE, 'expected_revision': 0}

def inquiry():
    return {'id': str(uuid.uuid4()), 'customer_id': CUSTOMER, 'manager_subject': 'author',
        'status': 'draft', 'cargo_description': 'Оборудование',
        'ready_date': '2026-11-10', 'desired_delivery_date': '2026-11-20',
        'route_segments': [], 'expected_revision': 0}

async def test_card_unique_customer_and_revision(api):
    await settings(api)
    value = card()
    saved = (await api.post('/api/v1/sales/cards/save', value))['item']
    assert saved['revision'] == 1
    assert saved['organization_id'] == ORG
    await api.post('/api/v1/sales/cards/save', card(), expected=409)
    await api.post('/api/v1/sales/cards/save', value, expected=409)
    value.update(expected_revision=1, next_action_date='2026-11-11')
    saved = (await api.post('/api/v1/sales/cards/save', value))['item']
    assert saved['next_action_date'] == '2026-11-11'
    history = await api.post('/api/v1/sales/cards/history', {'id': value['id']})
    assert len(history['items']) == 2
    assert all(item['snapshot']['organization_id'] == ORG for item in history['items'])
    await api.post('/api/v1/sales/cards/get', {'id': value['id']}, user='other', expected=404)

async def test_sales_permissions(api):
    await api.post('/api/v1/sales/cards/list', {}, roles=[], expected=403)
    await api.post('/api/v1/sales/cards/save', card(), roles=['sales.read'], expected=403)

async def test_inquiry_dates_and_agreement(api):
    value = inquiry()
    value['desired_delivery_date'] = '2026-11-01'
    await api.post('/api/v1/sales/inquiries/save', value, expected=400)
    value['desired_delivery_date'] = '2026-11-20'
    saved = (await api.post('/api/v1/sales/inquiries/save', value))['item']
    await api.post('/api/v1/sales/inquiries/create-shipment',
        {'id': value['id'], 'expected_revision': 1}, expected=409)
    value.update(expected_revision=1, status='agreed')
    saved = (await api.post('/api/v1/sales/inquiries/save', value))['item']
    assert saved['status'] == 'agreed'
    value.update(expected_revision=2, cargo_description='Другое')
    await api.post('/api/v1/sales/inquiries/save', value, expected=409)
    value['status'] = 'in_progress'
    await api.post('/api/v1/sales/inquiries/save', value)

@pytest.mark.uservice_oneshot(config_hooks=['configure_dispatcher'])
async def test_creation_retry_freezes_snapshot(api, mockserver):
    import asyncio
    @mockserver.json_handler('/service-token')
    def token(request):
        return {'access_token': 'sales-delivery-token'}
    value = inquiry()
    await api.post('/api/v1/sales/inquiries/save', value)
    value.update(status='agreed', expected_revision=1)
    await api.post('/api/v1/sales/inquiries/save', value)
    calls = []
    @mockserver.handler('/api/internal/v1/shipments/ensure-created')
    def create(request):
        calls.append(copy.deepcopy(request.json))
        if len(calls) == 1:
            return mockserver.make_response(status=503, json={'error':'unavailable','message':'Повторите'})
        return mockserver.make_response(status=200, json={'shipment': {
            'id': SHIPMENT, 'revision': 1, 'number': 'S-1', 'customer_id': CUSTOMER,
            'shipment_date': '2026-11-10', 'status': 'Новая', 'route_segments': []}})
    command = {'id': value['id'], 'expected_revision': 2}
    result = await api.post('/api/v1/sales/inquiries/create-shipment', command)
    assert result['item']['converting']
    saved = (await api.post('/api/v1/sales/inquiries/get', {'id':value['id']}))['item']
    assert saved['converting']
    value.update(expected_revision=2, status='in_progress')
    await api.post('/api/v1/sales/inquiries/save', value, expected=409)
    await api.post('/api/v1/sales/inquiries/create-shipment', command)
    for _ in range(200):
        result = await api.post('/api/v1/sales/inquiries/get', {'id':value['id']})
        if result['item'].get('shipment_id'):
            break
        await asyncio.sleep(0.05)
    assert result['item']['shipment_id'] == SHIPMENT
    assert calls[0] == calls[1]
    assert calls[0]['shipment']['idempotency_key'] == calls[0]['source']['type'] + ':' + value['id'] + ':2'
    assert calls[0]['shipment']['logist'] is None
    assert calls[0]['source'] == {'type': 'sales.inquiry', 'id': value['id']}
    assert calls[0]['shipment']['customer_id'] == CUSTOMER
    assert calls[0]['shipment']['cargo_description'] == value['cargo_description']
    assert calls[0]['shipment']['reference'] == result['item']['number']
    assert 'organization_id' not in calls[0]['shipment']
    assert 'status' not in calls[0]['shipment']
    await api.post('/api/v1/sales/inquiries/create-shipment', command)
    assert len(calls) == 2

async def test_registry_filters_and_keyset(api):
    for number in range(3):
        value = inquiry()
        value['cargo_description'] = f'Груз {number}'
        await api.post('/api/v1/sales/inquiries/save', value)
    first = await api.post('/api/v1/sales/inquiries/list', {'sort':'number','limit':2})
    assert [item['number'] for item in first['items']] == ['ENQ-1','ENQ-2']
    assert all(item['organization_id'] == ORG for item in first['items'])
    second = await api.post('/api/v1/sales/inquiries/list', {'sort':'number','limit':2,'cursor':first['next_cursor']})
    assert [item['number'] for item in second['items']] == ['ENQ-3']
    filtered = await api.post('/api/v1/sales/inquiries/list', {'filters':[{'field':'cargo_description','query':'Груз 1'}]})
    assert len(filtered['items']) == 1
    await api.post('/api/v1/sales/inquiries/list', {'sort':'status','cursor':first['next_cursor']}, expected=400)
    assert not (await api.post('/api/v1/sales/inquiries/list', {}, user='other'))['items']

async def test_disabled_stage_preserves_existing_card(api):
    await settings(api)
    value = card()
    await api.post('/api/v1/sales/cards/save',value)
    await settings(api,[{'id':STAGE,'name':'Знакомство','enabled':False}],revision=1)
    await api.post('/api/v1/sales/cards/save',{**value,'expected_revision':1,'note':'История сохранена'})
    await api.post('/api/v1/sales/cards/save',card(),expected=400)

@pytest.mark.uservice_oneshot(config_hooks=['configure_dispatcher'])
async def test_permanent_rejection_can_be_corrected(api,mockserver):
    import asyncio
    @mockserver.json_handler('/service-token')
    def token(request):
        return {'access_token':'sales-delivery-token'}
    @mockserver.handler('/api/internal/v1/shipments/ensure-created')
    def create(request):
        return mockserver.make_response(status=400,json={'error':'invalid_route','message':'Исправьте маршрут'})
    notifications = []
    @mockserver.handler('/api/internal/v1/events/publish')
    def notify(request):
        notifications.append(request.json)
        if len(notifications) == 1:
            return mockserver.make_response(status=503, json={'error': 'unavailable', 'message': 'Повторите'})
        return mockserver.make_response(status=200, json={'accepted': True})
    value=inquiry()
    await api.post('/api/v1/sales/inquiries/save',value)
    value.update(status='agreed',expected_revision=1)
    await api.post('/api/v1/sales/inquiries/save',value)
    await api.post('/api/v1/sales/inquiries/create-shipment',{'id':value['id'],'expected_revision':2})
    for _ in range(100):
        result=(await api.post('/api/v1/sales/inquiries/get',{'id':value['id']}))['item']
        if result.get('conversion_error'): break
        await asyncio.sleep(.05)
    assert result['conversion_error']
    assert not result['converting']
    for _ in range(100):
        if len(notifications) >= 2:
            break
        await asyncio.sleep(.05)
    assert len(notifications) == 2
    assert notifications[0] == notifications[1]
    assert notifications[0]['type'] == 'sales.inquiry.shipment_creation_failed'
    assert notifications[0]['payload']['inquiry_id'] == value['id']
    assert notifications[0]['payload']['recipient_subject'] == 'author'

    value.update(status='in_progress',expected_revision=result['revision'])
    corrected=(await api.post('/api/v1/sales/inquiries/save',value))['item']
    assert not corrected.get('conversion_error')

async def test_inquiry_numbering_settings(api):
    configured = await api.post('/api/admin/sales/settings/update', {
        'organization_id': ORG, 'expected_revision': 0, 'stages': [],
        'inquiry_number_template': 'З-{sequence}/{yyyy}', 'reset_number_yearly': True})
    assert configured['inquiry_number_template'] == 'З-{sequence}/{yyyy}'
    first = (await api.post('/api/v1/sales/inquiries/save', inquiry()))['item']
    assert first['number'].startswith('З-1/')
    await api.post('/api/admin/sales/settings/update', {
        'organization_id': ORG, 'expected_revision': 1, 'stages': [],
        'inquiry_number_template': 'REQ-{sequence}', 'reset_number_yearly': True})
    second = (await api.post('/api/v1/sales/inquiries/save', inquiry()))['item']
    assert second['number'] == 'REQ-2'
    unchanged = (await api.post('/api/v1/sales/inquiries/get', {'id':first['id']}))['item']
    assert unchanged['number'] == first['number']
    await api.post('/api/admin/sales/settings/update', {
        'organization_id': ORG, 'expected_revision': 2, 'stages': [],
        'inquiry_number_template': 'invalid'}, expected=400)

async def test_stage_filter_labels_and_search(api):
    await settings(api, [{'id': STAGE, 'name': 'First contact', 'enabled': True}])
    await api.post('/api/v1/sales/cards/save', card())
    body = {'registry': {}, 'field': 'stage_id'}
    values = await api.post('/api/v1/sales/cards/filter-values', body)
    assert values['values'] == [{'value': STAGE, 'label': 'First contact'}]
    found = await api.post('/api/v1/sales/cards/filter-values', {**body, 'query': 'FIRST'})
    assert found['values'] == values['values']
    missing = await api.post('/api/v1/sales/cards/filter-values', {**body, 'query': 'нет такого'})
    assert missing['values'] == []

async def test_contact_history_updates_next_action_once_and_checks_access(api):
    await settings(api)
    value = card()
    saved = (await api.post('/api/v1/sales/cards/save', value))['item']
    contact = {'id': str(uuid.uuid4()), 'card_id': saved['id'], 'expected_revision': 1,
        'kind': 'call', 'contact_date': '2026-11-08', 'result': 'Обсудили сроки',
        'next_action': 'Отправить предложение', 'next_action_date': '2026-11-10'}
    result = await api.post('/api/v1/sales/contacts/add', contact)
    assert result['card']['revision'] == 2
    assert result['card']['next_action'] == contact['next_action']
    assert result['item']['author_subject'] == 'author'
    assert (await api.post('/api/v1/sales/contacts/add', contact)) == result
    await api.post('/api/v1/sales/contacts/add', {**contact, 'result': 'Другая запись'}, expected=409)
    await api.post('/api/v1/sales/contacts/add', {**contact, 'id': str(uuid.uuid4())}, expected=409)
    records = await api.post('/api/v1/sales/contacts/list', {'card_id': saved['id']})
    assert records['items'] == [result['item']]
    assert 'request' not in records['items'][0]
    await api.post('/api/v1/sales/contacts/list', {'card_id': saved['id']}, user='other', expected=404)
    await api.post('/api/v1/sales/contacts/add', contact, roles=['sales.read'], expected=403)
    await api.post('/api/v1/sales/contacts/list', {'card_id': saved['id']}, roles=[], expected=403)
    history = (await api.post('/api/v1/sales/cards/history', {'id': saved['id']}))['items']
    assert len(history) == 2
    assert history[-1]['snapshot']['next_action'] == contact['next_action']

async def test_contacts_pagination_and_optional_next_action(api):
    await settings(api)
    value = card()
    saved = (await api.post('/api/v1/sales/cards/save', value))['item']
    base = {'card_id': saved['id'], 'expected_revision': 1, 'kind': 'email',
        'contact_date': '2026-11-07', 'result': 'Написали клиенту'}
    for day in ['07', '08', '06']:
        await api.post('/api/v1/sales/contacts/add', {**base, 'id': str(uuid.uuid4()), 'contact_date': f'2026-11-{day}'})
    first = await api.post('/api/v1/sales/contacts/list', {'card_id': saved['id'], 'limit': 2})
    assert [item['contact_date'] for item in first['items']] == ['2026-11-08', '2026-11-07']
    second = await api.post('/api/v1/sales/contacts/list', {'card_id': saved['id'], 'limit': 2, 'before': first['next_before']})
    assert [item['contact_date'] for item in second['items']] == ['2026-11-06']
    assert 'next_before' not in second
    current = (await api.post('/api/v1/sales/cards/get', {'id': saved['id']}))['item']
    assert current['revision'] == 1
    for extra in [{'next_action': 'Позвонить'}, {'result': '   '}, {'kind': 'unknown'}]:
        await api.post('/api/v1/sales/contacts/add', {**base, 'id': str(uuid.uuid4()), **extra}, expected=400)

async def test_contact_corrections_follow_source_and_preserve_newer_action(api):
    await settings(api)
    value = card()
    saved = (await api.post('/api/v1/sales/cards/save', value))['item']
    original = {'id': str(uuid.uuid4()), 'card_id': saved['id'], 'expected_revision': 1,
        'kind': 'call', 'contact_date': '2026-11-08', 'result': 'Опечатка',
        'next_action': 'Позвонить', 'next_action_date': '2026-11-10'}
    created = (await api.post('/api/v1/sales/contacts/add', original))['item']
    correction = {**original, 'expected_revision': created['revision'], 'kind': 'meeting',
        'result': 'Исправленный результат встречи', 'contact_date': '2026-11-07',
        'next_action': 'Написать', 'next_action_date': '2026-11-11'}
    updated = (await api.post('/api/v1/sales/contacts/update', correction))['item']
    assert updated['revision'] == 2
    assert updated['result'] == correction['result']
    assert updated['author_subject'] == created['author_subject']
    assert updated['created_at'] == created['created_at']
    current = (await api.post('/api/v1/sales/cards/get', {'id': saved['id']}))['item']
    assert current['next_action'] == correction['next_action']
    assert current['next_action_date'] == correction['next_action_date']
    assert current['revision'] == 3
    history = (await api.post('/api/v1/sales/contacts/history', {'id': created['id']}))['items']
    assert [item['revision'] for item in history] == [1, 2]
    assert history[0]['snapshot']['result'] == 'Опечатка'
    assert history[1]['snapshot']['result'] == correction['result']
    await api.post('/api/v1/sales/contacts/update', correction, expected=409)
    await api.post('/api/v1/sales/contacts/update', correction, roles=['sales.read'], expected=403)
    await api.post('/api/v1/sales/contacts/update', correction, user='other', expected=404)
    assert (await api.post('/api/v1/sales/contacts/history', {'id': created['id']}, user='other'))['items'] == []
    invalid = {**correction, 'expected_revision': 2, 'result': '   '}
    await api.post('/api/v1/sales/contacts/update', invalid, expected=400)

    manual = {**value, 'expected_revision': current['revision'],
        'next_action': 'Новое самостоятельное действие', 'next_action_date': '2026-11-20'}
    await api.post('/api/v1/sales/cards/save', manual)
    await api.post('/api/v1/sales/contacts/update', {**correction, 'expected_revision': 2,
        'next_action': 'Исправление старого контакта'})
    current = (await api.post('/api/v1/sales/cards/get', {'id': saved['id']}))['item']
    assert current['next_action'] == manual['next_action']
    assert current['next_action_date'] == manual['next_action_date']
    newer = {**original, 'id': str(uuid.uuid4()), 'expected_revision': current['revision'],
        'next_action': manual['next_action'], 'next_action_date': manual['next_action_date']}
    await api.post('/api/v1/sales/contacts/add', newer)
    await api.post('/api/v1/sales/contacts/update', {**correction, 'expected_revision': 3,
        'next_action': 'Не должно затереть новый контакт'})
    current = (await api.post('/api/v1/sales/cards/get', {'id': saved['id']}))['item']
    assert current['next_action'] == newer['next_action']
    cleared = {key: value for key, value in newer.items()
        if key not in ('next_action', 'next_action_date')}
    await api.post('/api/v1/sales/contacts/update', {**cleared, 'expected_revision': 1})
    current = (await api.post('/api/v1/sales/cards/get', {'id': saved['id']}))['item']
    assert 'next_action' not in current and 'next_action_date' not in current

async def test_work_filters_and_contact_presence_are_composable(api):
    await settings(api)
    cards = {}
    for name, action, date in [
        ('none', None, None), ('blank', '   ', None),
        ('overdue', 'Позвонить', '2026-11-07'),
        ('today', 'Встреча', '2026-11-08'), ('future', 'Письмо', '2026-11-09')]:
        value = {**card(), 'customer_id': str(uuid.uuid4())}
        if action is not None:
            value['next_action'] = action
        if date:
            value['next_action_date'] = date
        cards[name] = (await api.post('/api/v1/sales/cards/save', value))['item']
    await api.post('/api/v1/sales/contacts/add', {'id': str(uuid.uuid4()),
        'card_id': cards['today']['id'], 'expected_revision': 1, 'kind': 'call',
        'contact_date': '2026-11-08', 'result': 'Обсудили задачу'})
    async def selected(filters, user='author'):
        result = await api.post('/api/v1/sales/cards/list', {'filters': filters}, user=user)
        return {item['id'] for item in result['items']}
    assert await selected([{'field': 'next_action', 'empty': True}]) == {
        cards['none']['id'], cards['blank']['id']}
    assert await selected([{'field': 'next_action_date', 'date_to': '2026-11-07'}]) == {
        cards['overdue']['id']}
    assert await selected([{'field': 'next_action_date', 'date_from': '2026-11-08',
        'date_to': '2026-11-08'}]) == {cards['today']['id']}
    no_contacts = {'field': 'has_contacts', 'values': ['false']}
    assert await selected([no_contacts]) == {item['id'] for name, item in cards.items() if name != 'today'}
    assert await selected([{'field': 'has_contacts', 'values': ['true']}]) == {cards['today']['id']}
    assert await selected([no_contacts, {'field': 'next_action_date', 'date_to': '2026-11-08'}]) == {cards['overdue']['id']}
    assert await selected([{'field': 'manager_subject', 'values': ['author']}]) == {item['id'] for item in cards.values()}
    assert not await selected([{'field': 'manager_subject', 'values': ['nobody']}])
    assert not await selected([no_contacts], user='other')
    metadata = await api.post('/api/v1/sales/cards/filter-values', {'registry': {}})
    assert any(field['id'] == 'has_contacts' and field['kind'] == 'boolean' for field in metadata['fields'])
    facet = await api.post('/api/v1/sales/cards/filter-values', {
        'registry': {'filters': [no_contacts]}, 'field': 'manager_subject'})
    assert [value['value'] for value in facet['values']] == ['author']


@pytest.mark.parametrize('status, expected_status, code', [
    (403, 403, 'reference_forbidden'),
    (404, 404, 'reference_not_found'),
    (503, 502, 'reference_unavailable'),
])
async def test_creation_reference_failure_keeps_inquiry_editable(
        api, mockserver, status, expected_status, code):
    value = inquiry()
    await api.post('/api/v1/sales/inquiries/save', value)
    value.update(status='agreed', expected_revision=1)
    await api.post('/api/v1/sales/inquiries/save', value)

    @mockserver.handler('/api/v1/counterparties/get')
    def unavailable_customer(request):
        return mockserver.make_response(
            status=status, json={'error': 'unavailable', 'message': 'Customer unavailable'})

    response = await api.post('/api/v1/sales/inquiries/create-shipment',
                             {'id': value['id'], 'expected_revision': 2}, expected=expected_status)
    assert response['error'] == code
    current = (await api.post('/api/v1/sales/inquiries/get', {'id': value['id']}))['item']
    assert not current['converting']
    assert not current.get('shipment_id')
    assert current['revision'] == 2


async def test_creation_checks_customer_logist_before_queuing(api, mockserver):
    value = inquiry()
    await api.post('/api/v1/sales/inquiries/save', value)
    value.update(status='agreed', expected_revision=1)
    await api.post('/api/v1/sales/inquiries/save', value)

    @mockserver.json_handler('/api/v1/counterparties/get')
    def customer_with_logist(request):
        return {'counterparty': {
            'id': CUSTOMER, 'name': 'Customer', 'revision': 1, 'archived': False,
            'roles': [], 'own_company': False, 'contacts': [], 'logist': 'inactive-logist',
            'legal_name': None, 'country_code': 'RU', 'tax_id': None,
            'registration_id': None, 'legal_address': None,
        }}

    @mockserver.json_handler('/api/organizations/employees/list')
    def inactive_logist(request):
        assert request.json['subject'] == 'inactive-logist'
        assert request.json['organization_id'] == ORG
        return {'items': []}

    response = await api.post('/api/v1/sales/inquiries/create-shipment',
                             {'id': value['id'], 'expected_revision': 2}, expected=400)
    assert response['error'] == 'invalid_logist'
    current = (await api.post('/api/v1/sales/inquiries/get', {'id': value['id']}))['item']
    assert not current['converting']
    assert current['revision'] == 2
