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
    await api.post('/api/v1/sales/cards/save', card(), expected=409)
    await api.post('/api/v1/sales/cards/save', value, expected=409)
    value.update(expected_revision=1, next_action_date='2026-11-11')
    saved = (await api.post('/api/v1/sales/cards/save', value))['item']
    assert saved['next_action_date'] == '2026-11-11'
    history = await api.post('/api/v1/sales/cards/history', {'id': value['id']})
    assert len(history['items']) == 2
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
    assert calls[0]['shipment']['idempotency_key'] == 'sales-inquiry:' + value['id'] + ':2'
    assert calls[0]['shipment']['logist'] is None
    await api.post('/api/v1/sales/inquiries/create-shipment', command)
    assert len(calls) == 2

async def test_registry_filters_and_keyset(api):
    for number in range(3):
        value = inquiry()
        value['cargo_description'] = f'Груз {number}'
        await api.post('/api/v1/sales/inquiries/save', value)
    first = await api.post('/api/v1/sales/inquiries/list', {'sort':'number','limit':2})
    assert [item['number'] for item in first['items']] == ['ENQ-1','ENQ-2']
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
