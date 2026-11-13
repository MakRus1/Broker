import asyncio
import copy
from uuid import uuid4
import pytest
from test_sales import inquiry, references, ORG, CUSTOMER, SHIPMENT

BASE = '/api/v1/sales/proposals/'
LABEL = '66666666-6666-4666-8666-666666666666'

@pytest.fixture(autouse=True)
def finance(mockserver):
    state = {'rate': 90000000, 'name': 'Фрахт'}
    @mockserver.json_handler('/api/v1/finance/settings/get')
    def settings(request):
        return {'reporting_currency':'RUB','revision':0,'outgoing_number_template':'X-{sequence}',
                'incoming_number_template':'Y-{sequence}','reset_numbers_yearly':False}
    @mockserver.json_handler('/api/v1/exchange-rates/get')
    def rate(request):
        return {'date':request.json['date'],'source':'cbr','rate_millionths':state['rate']}
    @mockserver.json_handler('/api/v1/financial-labels/get')
    def label(request):
        return {'label':{'id':LABEL,'name':state['name'],'direction':'both','unit':'transportation',
               'currency':'USD','vat_mode':'exempt','vat_rate_basis_points':None,
               'note':None,'archived':False,'revision':1}}
    return state

async def draft(api, **fields):
    incoming = {**inquiry(), **fields}
    await api.post('/api/v1/sales/inquiries/save',incoming)
    price = {'price_minor':230000,'currency':'USD','price_mode':'gross','vat_mode':'rate','vat_rate_basis_points':2000}
    value={'id':str(uuid4()),'inquiry_id':incoming['id'],'expected_revision':0,'title':'Морем',
           'calculation_date':'2026-11-08','valid_until':'2026-11-30','lines':[{'id':str(uuid4()),'label_id':LABEL,
           'unit':'transportation','quantity_milli':1000,'sale':price,
           'purchase':{**price,'price_minor':200000},'supplier_id':CUSTOMER}]}
    return incoming,value

async def test_proposal_rounding_history_and_frozen_snapshot(api,finance):
    incoming,value=await draft(api)
    saved=(await api.post(BASE+'save',value))['item']
    assert saved['lines'][0]['sale']=={'net_minor':191667,'vat_minor':38333,'total_minor':230000}
    assert saved['reporting']['sale']['net_minor']==17250030
    assert saved['customer_name']=='Клиент'
    assert saved['lines'][0]['label_name']=='Фрахт'
    await api.post(BASE+'save',value,expected=409)
    value['expected_revision']=1
    value['title']='Вариант 2'
    saved=(await api.post(BASE+'save',value))['item']
    finance.update(rate=100000000,name='Новое имя')
    offered=(await api.post(BASE+'action',{'id':value['id'],'expected_revision':2,'action':'offer'}))['item']
    assert offered['rates']==saved['rates']
    assert offered['lines']==saved['lines']
    await api.post(BASE+'save',{**value,'expected_revision':3},expected=409)
    accepted=(await api.post(BASE+'action',{'id':value['id'],'expected_revision':3,'action':'accept'}))['item']
    assert accepted['status']=='accepted'
    current=(await api.post('/api/v1/sales/inquiries/get',{'id':incoming['id']}))['item']
    assert current['accepted_proposal_id']==value['id']
    assert current['accepted_proposal_revision']==4
    history=(await api.post(BASE+'history',{'id':value['id']}))['items']
    assert [v['snapshot']['revision'] for v in history]==[1,2,3,4]
    assert history[0]['snapshot']['input']['title']=='Морем'
    assert (await api.post(BASE+'list',{'id':incoming['id']},user='other'))['items']==[]
    await api.post(BASE+'action',{'id':value['id'],'expected_revision':4,'action':'accept'},user='other',expected=404)
    await api.post(BASE+'save',value,roles=['sales.read'],expected=403)

async def test_cargo_required_before_calculating_proposal(api):
    incoming, value = await draft(api, cargo_description='')
    response = await api.post(BASE + 'save', value, expected=400)
    assert response['error'] == 'cargo_required'
    incoming.update(expected_revision=1, cargo_description='Оборудование')
    await api.post('/api/v1/sales/inquiries/save', incoming)
    await api.post(BASE + 'save', value)
    await api.post(BASE + 'action', {
        'id': value['id'], 'expected_revision': 1, 'action': 'offer',
    })
    accepted = (await api.post(BASE + 'action', {
        'id': value['id'], 'expected_revision': 2, 'action': 'accept',
    }))['item']
    assert accepted['status'] == 'accepted'
    assert accepted['inquiry_revision'] == 2


async def test_proposal_validation(api):
    incoming,value=await draft(api)
    bad=copy.deepcopy(value);bad['lines'][0]['sale']['vat_mode']='exempt'
    await api.post(BASE+'save',bad,expected=400)
    bad=copy.deepcopy(value);bad['lines'][0]['route_segment_id']=str(uuid4())
    await api.post(BASE+'save',bad,expected=400)
    await api.post(BASE+'save',{**value,'valid_until':'2026-10-01'},expected=400)
    bad=copy.deepcopy(value);bad['lines'].append(copy.deepcopy(bad['lines'][0]))
    await api.post(BASE+'save',bad,expected=400)
    value['lines'][0].pop('supplier_id')
    await api.post(BASE+'save',value)
    await api.post(BASE+'action',{'id':value['id'],'expected_revision':1,'action':'offer'})
    await api.post(BASE+'action',{'id':value['id'],'expected_revision':2,'action':'accept'},expected=400)

@pytest.mark.uservice_oneshot(config_hooks=['configure_dispatcher'])
async def test_accepted_proposal_transfer_retries_without_changing_import(api,mockserver):
    @mockserver.json_handler('/service-token')
    def token(request):return {'access_token':'sales-delivery-token'}
    @mockserver.json_handler('/api/internal/v1/shipments/ensure-created')
    def shipment(request):
        return {'shipment':{'id':SHIPMENT,'revision':1,'number':'S-1','customer_id':CUSTOMER,
                'shipment_date':'2026-11-08','status':'Новая','route_segments':[]}}
    deliveries=[]
    @mockserver.handler('/api/internal/v1/service-charges/apply-import')
    def transfer(request):
        deliveries.append(copy.deepcopy(request.json))
        if len(deliveries)==1:return mockserver.make_response(status=503,json={'error':'unavailable','message':'Retry'})
        return mockserver.make_response(status=200,json={'imported':True})
    incoming,value=await draft(api)
    await api.post(BASE+'save',value)
    await api.post(BASE+'action',{'id':value['id'],'expected_revision':1,'action':'offer'})
    await api.post(BASE+'action',{'id':value['id'],'expected_revision':2,'action':'accept'})
    await api.post('/api/v1/sales/inquiries/create-shipment',{'id':incoming['id'],'expected_revision':2})
    for _ in range(200):
        item=(await api.post('/api/v1/sales/inquiries/get',{'id':incoming['id']}))['item']
        if item.get('shipment_id'):break
        await asyncio.sleep(.05)
    assert item['shipment_id']==SHIPMENT
    assert len(deliveries)==2
    assert deliveries[0]==deliveries[1]
    lines=deliveries[0]['items']
    assert len({line['charge']['id'] for line in lines})==2
    assert {line['charge']['direction'] for line in lines}=={'income','expense'}
    assert all(line['charge']['shipment_id']==SHIPMENT for line in lines)
    assert lines[0]['counterparty_name']=='Клиент'


@pytest.mark.parametrize('rate,price,expected', [(500000, 1, 1), (1499999, 1, 1), (1500000, 1, 2), (90000000, 100000000, 9000000000)])
async def test_reporting_conversion_rounds_half_up(api, finance, rate, price, expected):
    _, value = await draft(api)
    finance['rate'] = rate
    line = value['lines'][0]
    line.pop('purchase')
    line['sale'].update(price_minor=price, price_mode='net', vat_mode='exempt')
    line['sale'].pop('vat_rate_basis_points')
    saved = (await api.post(BASE + 'save', value))['item']
    assert saved['reporting']['sale']['total_minor'] == expected


async def test_inquiry_changed_during_calculation_does_not_save_proposal(api, mockserver, pgsql):
    incoming, value = await draft(api)

    @mockserver.json_handler('/api/v1/exchange-rates/get')
    def rate(request):
        cursor = pgsql['db_1'].cursor()
        cursor.execute(
            'UPDATE sales_inquiries SET revision = revision + 1 WHERE id = %s',
            (incoming['id'],),
        )
        return {'date': request.json['date'], 'source': 'cbr', 'rate_millionths': 90000000}

    result = await api.post(BASE + 'save', value, expected=409)
    assert result['error'] == 'inquiry_changed'
    assert (await api.post(BASE + 'list', {'id': incoming['id']}))['items'] == []


@pytest.mark.parametrize('status', [403, 404, 503, 200])
async def test_calculation_dependency_failure_does_not_save_proposal(api, mockserver, status):
    incoming, value = await draft(api)

    @mockserver.handler('/api/v1/financial-labels/get')
    def unavailable(request):
        if status == 200:
            return mockserver.make_response('invalid-response', status=status)
        return mockserver.make_response(status=status, json={'error': 'unavailable', 'message': 'Ошибка справочника'})

    expected = status if status in (403, 404) else 502
    result = await api.post(BASE + 'save', value, expected=expected)
    assert result['error'] in ('reference_forbidden', 'reference_not_found', 'reference_unavailable')
    assert (await api.post(BASE + 'list', {'id': incoming['id']}))['items'] == []

async def test_repeated_lines_load_each_reference_once(api, finance, mockserver):
    incoming, value = await draft(api)
    supplier = str(uuid4())
    calls = {'labels': 0, 'suppliers': 0}

    @mockserver.json_handler('/api/v1/financial-labels/get')
    def label(request):
        calls['labels'] += 1
        return {'label': {'id': LABEL, 'name': 'Фрахт', 'direction': 'both',
            'unit': 'transportation', 'currency': 'USD', 'vat_mode': 'exempt',
            'vat_rate_basis_points': None, 'note': None, 'archived': False, 'revision': 1}}

    @mockserver.json_handler('/api/v1/counterparties/get')
    def party(request):
        if request.json['id'] == supplier:
            calls['suppliers'] += 1
        return {'counterparty': {'id': request.json['id'], 'name': 'Клиент',
            'revision': 1, 'archived': False, 'roles': [], 'own_company': False,
            'contacts': [], 'legal_name': None, 'country_code': 'RU', 'tax_id': None,
            'registration_id': None, 'legal_address': None}}

    value['lines'][0]['supplier_id'] = supplier
    second = copy.deepcopy(value['lines'][0])
    second['id'] = str(uuid4())
    value['lines'].append(second)
    saved = (await api.post(BASE + 'save', value))['item']
    assert len(saved['lines']) == 2
    assert calls == {'labels': 1, 'suppliers': 1}

    value['expected_revision'] = saved['revision']
    await api.post(BASE + 'save', value)
    assert calls == {'labels': 2, 'suppliers': 2}


async def test_cached_label_is_validated_for_each_line(api, finance, mockserver):
    incoming, value = await draft(api)
    calls = []

    @mockserver.json_handler('/api/v1/financial-labels/get')
    def label(request):
        calls.append(request.json['id'])
        return {'label': {'id': LABEL, 'name': 'Доход', 'direction': 'income',
            'unit': 'transportation', 'currency': 'USD', 'vat_mode': 'exempt',
            'vat_rate_basis_points': None, 'note': None, 'archived': False, 'revision': 1}}

    purchase_line = copy.deepcopy(value['lines'][0])
    purchase_line['id'] = str(uuid4())
    value['lines'][0].pop('purchase')
    value['lines'][0].pop('supplier_id')
    value['lines'].append(purchase_line)
    response = await api.post(BASE + 'save', value, expected=400)
    assert response['error'] == 'invalid_label'
    assert calls == [LABEL]
