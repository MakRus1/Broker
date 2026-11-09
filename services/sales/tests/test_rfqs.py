import copy
from uuid import uuid4
import pytest
from test_sales import inquiry, references
from test_proposals import LABEL, CUSTOMER, finance

BASE = '/api/v1/sales/rfqs/'

async def setup_rfq(api):
    incoming = inquiry()
    segment = {'id': str(uuid4()), 'type': 'sea'}
    incoming['route_segments'] = [segment]
    await api.post('/api/v1/sales/inquiries/save', incoming)
    value = {'id': str(uuid4()), 'inquiry_id': incoming['id'],
             'route_segment_id': segment['id'], 'title': 'Морской фрахт',
             'responsible_subject': 'author', 'status': 'in_progress',
             'quotes': [], 'expected_revision': 0}
    return incoming, value


def quote(price=10000):
    return {'id': str(uuid4()), 'title': 'Прямая линия', 'supplier_id': CUSTOMER,
            'valid_until': '2026-12-31', 'delivery_days': 30,
            'lines': [{'id': str(uuid4()), 'label_id': LABEL, 'unit': 'transportation',
                       'quantity_milli': 1000, 'price': {'price_minor': price, 'currency': 'USD',
                       'price_mode': 'net', 'vat_mode': 'exempt'}}]}


async def test_rfq_permissions_isolation_revision_and_history(api):
    incoming, value = await setup_rfq(api)
    saved = (await api.post(BASE+'save', value))['item']
    assert saved['revision'] == 1
    assert saved['route_segment']['id'] == value['route_segment_id']
    await api.post(BASE+'save', value, expected=409)
    await api.post(BASE+'get', {'id':value['id']}, user='other', expected=404)
    assert (await api.post(BASE+'list', {'id':incoming['id']}, user='other'))['items'] == []
    await api.post(BASE+'save', value, roles=['sales.read'], expected=403)
    value.update(expected_revision=1, quotes=[quote(), quote(11000)], status='ready')
    saved = (await api.post(BASE+'save', value))['item']
    assert len(saved['input']['quotes']) == 2
    assert saved['input']['quotes'][0]['supplier_name'] == 'Клиент'
    assert saved['input']['quotes'][0]['lines'][0]['label_name'] == 'Фрахт'
    history = (await api.post(BASE+'history', {'id':value['id']}))['items']
    assert [v['snapshot']['revision'] for v in history] == [1, 2]
    assert history[0]['snapshot']['input']['quotes'] == []
    bad = {**value, 'inquiry_id':str(uuid4()), 'expected_revision':2}
    await api.post(BASE+'save', bad, expected=404)


async def test_rfq_requires_current_segment_and_nonempty_ready_quotes(api):
    _, value = await setup_rfq(api)
    await api.post(BASE+'save', {**value,'route_segment_id':str(uuid4())}, expected=400)
    await api.post(BASE+'save', {**value,'status':'ready'}, expected=400)
    q = quote()
    await api.post(BASE+'save', {**value,'quotes':[q, q]}, expected=400)
    bad = copy.deepcopy(q)
    bad['lines'].append(copy.deepcopy(bad['lines'][0]))
    await api.post(BASE+'save', {**value,'quotes':[bad]}, expected=400)


async def test_proposal_uses_quote_snapshot_and_rejects_wrong_inquiry(api):
    incoming, value = await setup_rfq(api)
    value.update(quotes=[quote()], status='ready')
    await api.post(BASE+'save', value)
    q = value['quotes'][0]
    source = {'rfq_id':value['id'],'revision':1,'quote_id':q['id'],'line_id':q['lines'][0]['id']}
    line = {'id':str(uuid4()),'label_id':LABEL,'unit':'transportation','quantity_milli':1000,
            'sale':{**q['lines'][0]['price'],'price_minor':20000},'quote_source':source}
    proposal = {'id':str(uuid4()),'inquiry_id':incoming['id'],'expected_revision':0,
                'title':'Море','calculation_date':'2026-11-08','lines':[line]}
    saved = (await api.post('/api/v1/sales/proposals/save', proposal))['item']
    assert saved['input']['lines'][0]['purchase']['price_minor'] == 10000
    assert saved['input']['lines'][0]['supplier_id'] == CUSTOMER
    assert saved['input']['lines'][0]['route_segment_id'] == value['route_segment_id']
    value.update(expected_revision=1)
    value['quotes'][0]['lines'][0]['price']['price_minor'] = 15000
    await api.post(BASE+'save', value)
    proposal['expected_revision'] = 1
    saved = (await api.post('/api/v1/sales/proposals/save', proposal))['item']
    assert saved['input']['lines'][0]['purchase']['price_minor'] == 10000
    other = inquiry()
    await api.post('/api/v1/sales/inquiries/save', other)
    await api.post('/api/v1/sales/proposals/save', {**proposal,'inquiry_id':other['id']}, expected=400)
    proposal['calculation_date'] = '2027-01-01'
    await api.post('/api/v1/sales/proposals/save', proposal, expected=400)


@pytest.mark.uservice_oneshot(config_hooks=['configure_dispatcher'])
async def test_rfq_assignment_ready_notifications_retry_and_no_edit_spam(api, mockserver):
    import asyncio
    @mockserver.json_handler('/service-token')
    def token(request): return {'access_token':'sales-delivery-token'}
    calls=[]
    @mockserver.handler('/api/internal/v1/events/publish')
    def notify(request):
        calls.append(copy.deepcopy(request.json))
        return mockserver.make_response(status=503 if len(calls)==1 else 200,
            json={'error':'unavailable','message':'Retry'} if len(calls)==1 else {'accepted':True})
    _,value=await setup_rfq(api)
    await api.post(BASE+'save',value)
    for _ in range(200):
        if len(calls)>=2:break
        await asyncio.sleep(.05)
    assert len(calls)==2 and calls[0]==calls[1]
    assert calls[1]['type']=='sales.rfq.assigned'
    value.update(expected_revision=1,status='ready',quotes=[quote()])
    await api.post(BASE+'save',value)
    for _ in range(200):
        if len(calls)>=3:break
        await asyncio.sleep(.05)
    assert calls[2]['type']=='sales.rfq.ready'
    value.update(expected_revision=2,note='Уточнение')
    await api.post(BASE+'save',value)
    await asyncio.sleep(.2)
    assert len(calls)==3


async def test_combine_suppliers_for_different_legs_and_reject_changed_route(api):
    incoming, first = await setup_rfq(api)
    second_leg = {'id':str(uuid4()),'type':'air'}
    incoming.update(expected_revision=1,route_segments=[*incoming['route_segments'],second_leg])
    await api.post('/api/v1/sales/inquiries/save',incoming)
    second = {**first,'id':str(uuid4()),'route_segment_id':second_leg['id']}
    for rfq,supplier in [(first,CUSTOMER),(second,str(uuid4()))]:
        rfq.update(status='ready',quotes=[{**quote(),'supplier_id':supplier}])
        await api.post(BASE+'save',rfq)
    lines=[]
    for rfq in [first,second]:
        q=rfq['quotes'][0]
        lines.append({'id':str(uuid4()),'label_id':LABEL,'unit':'transportation','quantity_milli':1000,
                      'sale':q['lines'][0]['price'],'quote_source':{'rfq_id':rfq['id'],'revision':1,
                      'quote_id':q['id'],'line_id':q['lines'][0]['id']}})
    body={'id':str(uuid4()),'inquiry_id':incoming['id'],'expected_revision':0,
          'title':'Комбинированный маршрут','calculation_date':'2026-11-08','lines':lines}
    saved=(await api.post('/api/v1/sales/proposals/save',body))['item']
    result=saved['input']['lines']
    assert {v['route_segment_id'] for v in result}=={first['route_segment_id'],second_leg['id']}
    assert len({v['supplier_id'] for v in result})==2
    incoming.update(expected_revision=2)
    incoming['route_segments'][0]['type']='rail'
    await api.post('/api/v1/sales/inquiries/save',incoming)
    await api.post('/api/v1/sales/proposals/save',{**body,'expected_revision':1},expected=400)
