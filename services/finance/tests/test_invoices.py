"""Счета оформляют начисления, не меняя доходы/расходы и не допуская двойного включения."""
import asyncio
from uuid import uuid4
import pytest
from test_service_charges import references, scope, charge, PARTY, CONTRACT, SHIPMENT

BASE = '/api/v1/invoices/'

@pytest.fixture
def invoice_references(mockserver, references):
    @mockserver.json_handler('/api/v1/counterparties/get')
    def party(request):
        return {'counterparty': {
            'id': request.json['id'], 'name': 'Своя компания' if request.json['id'] == SHIPMENT else 'Клиент',
            'revision': 1, 'own_company': request.json['id'] == SHIPMENT,
            'archived': False, 'country_code': None, 'tax_id': None,
            'registration_id': None, 'legal_name': None, 'legal_address': None,
            'roles': [], 'contacts': [],
        }}
    return references


def invoice(ids, **changes):
    return dict(id=str(uuid4()), direction='outgoing', number='ИС-1', invoice_date='2026-10-30',
                due_date='2026-11-10', own_company_id=SHIPMENT, counterparty_id=PARTY,
                own_bank_account_id=None, counterparty_bank_account_id=None,
                contract_id=CONTRACT, currency='RUB', note=None, archived=False,
                service_charge_ids=ids, expected_revision=0, **changes)

async def saved_entry(api, charge, **changes):
    body = await charge(**changes)
    return body, (await api.post('/api/v1/service-charges/save', body))['charge']

async def test_invoice_totals_snapshot_and_entry_lock(api, charge, invoice_references, scope):
    body, item = await saved_entry(api, charge)
    value = invoice([item['id']])
    before = await api.post('/api/v1/service-charges/list', dict(scope=scope, after=None, limit=50, include_archived=False))
    saved = await api.post(BASE+'save', value)
    assert saved['invoice']['total_minor'] == item['total_minor'] == 12000
    assert saved['charges'] == [item]
    assert await api.post(BASE+'get', {'id':value['id']}) == saved
    after = await api.post('/api/v1/service-charges/list', dict(scope=scope, after=None, limit=50, include_archived=False))
    assert before['summary'] == after['summary']
    error = await api.post('/api/v1/service-charges/save', {**body, 'expected_revision':1, 'archived':True}, expected=409)
    assert error['error'] == 'service_charge_invoiced'
    await api.post(BASE+'save', invoice([item['id']]), expected=409)
    await api.post(BASE+'save', {**value, 'expected_revision':1, 'archived':True})
    await api.post('/api/v1/service-charges/save', {**body, 'expected_revision':1, 'price_minor':1})
    archived = await api.post(BASE+'get', {'id':value['id']})
    assert archived['charges'][0]['price_minor'] == 10000
    assert archived['invoice']['total_minor'] == 12000

@pytest.mark.parametrize('change', [dict(direction='incoming'),dict(currency='USD'),dict(contract_id=None),dict(counterparty_id=SHIPMENT)])
async def test_incompatible_entries(api, charge, invoice_references, change):
    _, item = await saved_entry(api, charge)
    await api.post(BASE+'save', {**invoice([item['id']]), **change}, expected=400)

async def test_assignment_race(api, charge, invoice_references):
    _, item = await saved_entry(api, charge)
    async def save():
        return await api.post(BASE+'save', invoice([item['id']]), expected=(200,409))
    results = await asyncio.gather(save(), save())
    assert sum('invoice' in result for result in results) == 1
    assert sum(result.get('error') == 'service_charge_invoiced' for result in results) == 1

async def test_revision_and_missing_charge_are_atomic(api, charge, invoice_references):
    _, item = await saved_entry(api, charge)
    value=invoice([item['id']]); await api.post(BASE+'save', value)
    await api.post(BASE+'save', value, expected=409)
    await api.post(BASE+'save', {**value,'expected_revision':1,'service_charge_ids':[str(uuid4())]}, expected=404)
    assert (await api.post(BASE+'get', {'id':value['id']}))['invoice']['revision'] == 1
    await api.post(BASE+'get', {'id':value['id']}, user='other', expected=404)
    await api.post(BASE+'get', {'id':value['id']}, roles=[], expected=403)

async def test_candidates_pagination_and_archive(api, charge, invoice_references):
    items=[(await saved_entry(api, charge))[1] for _ in range(3)]
    value=invoice([items[0]['id']]); await api.post(BASE+'save',value)
    query=dict(direction='outgoing',counterparty_id=PARTY,currency='RUB',contract_id=CONTRACT,invoice_id=None,after=None,limit=1)
    first=await api.post(BASE+'candidates',query)
    second=await api.post(BASE+'candidates',{**query,'after':first['next_after']})
    assert len(first['items'])==len(second['items'])==1
    assert first['items'][0]['id']!=second['items'][0]['id']
    assert items[0]['id'] not in [first['items'][0]['id'],second['items'][0]['id']]
    own=await api.post(BASE+'candidates',{**query,'invoice_id':value['id'],'limit':100})
    assert len(own['items'])==3
    listing=dict(direction='outgoing',limit=50,filters=[{'field':'archived','values':['false']},{'field':'number','query':'ИС'},{'field':'counterparty_id','values':[PARTY]}])
    assert len((await api.post(BASE+'list',listing))['items'])==1
    assert not (await api.post(BASE+'list',{**listing,'filters':[{'field':'number','query':'missing'}]}))['items']
    await api.post(BASE+'save',{**value,'archived':True,'expected_revision':1})
    assert not (await api.post(BASE+'list',listing))['items']
    assert len((await api.post(BASE+'list',{**listing,'filters':[{'field':'number','query':'ИС'}]}))['items'])==1

async def test_invoice_edit_releases_removed_service(api, charge, invoice_references):
    _, first=await saved_entry(api,charge)
    _, second=await saved_entry(api,charge)
    value=invoice([first['id'],second['id']])
    await api.post(BASE+'save',value)
    changed=await api.post(BASE+'save',{**value,'service_charge_ids':[first['id']],'expected_revision':1})
    assert changed['invoice']['total_minor']==first['total_minor']
    other=await api.post(BASE+'save',invoice([second['id']]))
    assert other['charges']==[second]
    await api.post(BASE+'save',{**value,'service_charge_ids':[second['id']],'expected_revision':1},expected=409)
    assert (await api.post(BASE+'get',{'id':value['id']}))==changed

async def test_edit_and_invoice_race_keep_amounts_consistent(api, charge, invoice_references):
    body, item=await saved_entry(api,charge)
    document=invoice([item['id']])
    edited,saved=await asyncio.gather(
        api.post('/api/v1/service-charges/save',{**body,'price_minor':20000,'expected_revision':1},expected=(200,409)),
        api.post(BASE+'save',document),
    )
    final=await api.post(BASE+'get',{'id':document['id']})
    assert final['invoice']['total_minor']==edited['charge']['total_minor']
    assert final['charges']==[edited['charge']]

@pytest.mark.parametrize('changes', [dict(number='   '),dict(invoice_date='1899-12-31'),dict(due_date='1899-12-31'),dict(own_company_id=PARTY)])
async def test_invalid_header(api, charge, invoice_references, changes):
    _, item=await saved_entry(api,charge)
    await api.post(BASE+'save',{**invoice([item['id']]),**changes},expected=400)

async def test_incoming_invoice(api, charge, invoice_references):
    _, item=await saved_entry(api,charge,direction='expense')
    value=invoice([item['id']]);value['direction']='incoming'
    saved=await api.post(BASE+'save',value)
    assert saved['invoice']['direction']=='incoming'
    assert saved['invoice']['total_minor']==12000

async def test_registry_filters_and_bound_cursor(api, charge, invoice_references):
    _, item=await saved_entry(api,charge)
    for number in ['S-1','S-10','S-2']:
        value=invoice([item['id']]);value.update(number=number,archived=True)
        await api.post(BASE+'save',value)
    query={'direction':'outgoing','sort':'number','limit':1,'filters':[]}
    items=[];cursor=None
    for _ in range(3):
        result=await api.post(BASE+'list',{**query,**({'cursor':cursor} if cursor else {})})
        items.extend(row['invoice']['number'] for row in result['items']);cursor=result.get('next_cursor')
    assert items==['S-1','S-2','S-10']
    result=await api.post(BASE+'list',query)
    await api.post(BASE+'list',{**query,'direction':'incoming','cursor':result['next_cursor']},expected=400)
    dates=await api.post(BASE+'list',{'direction':'outgoing','filters':[{'field':'invoice_date','date_from':'2026-10-30','date_to':'2026-10-30'}]})
    assert len(dates['items'])==3
    missing=await api.post(BASE+'list',{'direction':'outgoing','filters':[{'field':'due_date','empty':True}]})
    assert missing['items']==[]
    catalog=await api.post(BASE+'filter-values',{'registry':{}})
    assert {'number','own_company_id','counterparty_id','invoice_date','due_date','currency','archived'}<={field['id'] for field in catalog['fields']}
    values=await api.post(BASE+'filter-values',{'registry':{'direction':'outgoing'},'field':'number'})
    assert {row['value'] for row in values['values']}=={'s-1','s-2','s-10'}
    assert {row['label'] for row in values['values']}=={'S-1','S-2','S-10'}
    assert not (await api.post(BASE+'filter-values',{'registry':{'direction':'incoming'},'field':'number'}))['values']

async def test_add_service_from_service_card(api,charge,invoice_references):
    _, first=await saved_entry(api,charge)
    _, second=await saved_entry(api,charge)
    assert (await api.post('/api/v1/service-charges/get',{'id':second['id']}))['charge']==second
    value=invoice([first['id']]);await api.post(BASE+'save',value)
    added=await api.post(BASE+'add-service-charge',{'invoice_id':value['id'],'service_charge_id':second['id'],'expected_revision':1})
    assert added['invoice']['revision']==2
    assert added['invoice']['total_minor']==first['total_minor']+second['total_minor']
    assert {item['id'] for item in added['charges']}=={first['id'],second['id']}
    await api.post(BASE+'add-service-charge',{'invoice_id':value['id'],'service_charge_id':second['id'],'expected_revision':1},expected=409)
    await api.post(BASE+'add-service-charge',{'invoice_id':value['id'],'service_charge_id':second['id'],'expected_revision':2},expected=409)
    await api.post('/api/v1/service-charges/get',{'id':second['id']},user='other',expected=404)
    await api.post(BASE+'add-service-charge',{'invoice_id':value['id'],'service_charge_id':first['id'],'expected_revision':2},user='other',expected=404)

async def test_add_service_to_archived_or_incompatible_invoice(api,charge,invoice_references):
    _, first=await saved_entry(api,charge)
    _, expense=await saved_entry(api,charge,direction='expense')
    value=invoice([first['id']]);await api.post(BASE+'save',value)
    await api.post(BASE+'add-service-charge',{'invoice_id':value['id'],'service_charge_id':expense['id'],'expected_revision':1},expected=400)
    await api.post(BASE+'save',{**value,'expected_revision':1,'archived':True})
    await api.post(BASE+'add-service-charge',{'invoice_id':value['id'],'service_charge_id':first['id'],'expected_revision':2},expected=409)

async def test_service_list_exposes_invoice_number_and_date(api,charge,invoice_references,scope):
    _, item=await saved_entry(api,charge)
    value=invoice([item['id']]);await api.post(BASE+'save',value)
    rows=await api.post('/api/v1/service-charges/list',dict(scope=scope,after=None,limit=50,include_archived=False))
    assert rows['invoices']==[dict(service_charge_id=item['id'],invoice_id=value['id'],number=value['number'],invoice_date=value['invoice_date'],direction='outgoing',read_only=False)]

async def test_edit_invoiced_service_updates_invoice_atomically(api,charge,invoice_references,pgsql):
    body,item=await saved_entry(api,charge)
    _,other=await saved_entry(api,charge)
    document=invoice([item['id'],other['id']]);await api.post(BASE+'save',document)
    updated=(await api.post('/api/v1/service-charges/save',{**body,'expected_revision':1,'price_minor':20000,'note':'Исправлено'}))['charge']
    saved=await api.post(BASE+'get',{'id':document['id']})
    assert saved['invoice']['revision']==2
    assert saved['invoice']['total_minor']==updated['total_minor']+other['total_minor']
    assert saved['invoice']['net_minor']==updated['net_minor']+other['net_minor']
    assert saved['invoice']['vat_minor']==updated['vat_minor']+other['vat_minor']
    assert next(row for row in saved['charges'] if row['id']==item['id'])==updated
    await api.post('/api/v1/service-charges/save',{**body,'expected_revision':1,'price_minor':1},expected=409)
    await api.post('/api/v1/service-charges/save',{**body,'expected_revision':2,'invoice_currency':'USD'},expected=400)
    assert await api.post(BASE+'get',{'id':document['id']})==saved
    await api.post(BASE+'save',{**document,'expected_revision':1},expected=409)

async def test_detach_and_reassign_service_recalculate_both_invoices(api,charge,invoice_references):
    _,first=await saved_entry(api,charge)
    _,second=await saved_entry(api,charge)
    left=invoice([first['id']]);right=invoice([second['id']])
    await api.post(BASE+'save',left);await api.post(BASE+'save',right)
    moved=await api.post(BASE+'add-service-charge',{'service_charge_id':first['id'],'invoice_id':right['id'],'expected_revision':1,'previous_invoice_id':left['id']})
    assert moved['invoice']['total_minor']==24000
    old=await api.post(BASE+'get',{'id':left['id']})
    assert old['charges']==[] and old['invoice']['total_minor']==0 and old['invoice']['revision']==2
    await api.post(BASE+'detach-service-charge',{'service_charge_id':first['id'],'invoice_id':left['id']},expected=409)
    detached=await api.post(BASE+'detach-service-charge',{'service_charge_id':first['id'],'invoice_id':right['id']})
    assert detached['invoice']['total_minor']==12000 and detached['invoice']['revision']==3
    assert detached['charges']==[second]

async def test_reassign_into_new_invoice_is_atomic(api,charge,invoice_references):
    _,item=await saved_entry(api,charge)
    old=invoice([item['id']]);await api.post(BASE+'save',old)
    new=invoice([item['id']]);new['previous_invoice_id']=old['id']
    await api.post(BASE+'save',{**new,'direction':'incoming'},expected=400)
    assert (await api.post(BASE+'get',{'id':old['id']}))['invoice']['total_minor']==12000
    await api.post(BASE+'save',new)
    assert not (await api.post(BASE+'get',{'id':old['id']}))['charges']
    await api.post(BASE+'detach-service-charge',{'service_charge_id':item['id'],'invoice_id':new['id']},user='other',expected=404)

async def test_bulk_invoice_add_preserves_existing_services(api, charge, invoice_references):
    services = [(await saved_entry(api, charge))[1] for _ in range(3)]
    value = invoice([services[0]['id']])
    await api.post(BASE + 'save', value)
    combined = await api.post(BASE + 'save', {
        **value, 'expected_revision': 1,
        'service_charge_ids': [item['id'] for item in services],
    })
    assert {item['id'] for item in combined['charges']} == {item['id'] for item in services}
    assert combined['invoice']['total_minor'] == sum(item['invoice_total_minor'] for item in services)
    assert combined['invoice']['revision'] == 2


async def test_bulk_invoice_invalid_service_rolls_back_whole_selection(api, charge, invoice_references):
    _, first = await saved_entry(api, charge)
    _, incompatible = await saved_entry(api, charge, direction='expense')
    value = invoice([first['id'], incompatible['id']])
    await api.post(BASE + 'save', value, expected=400)
    await api.post(BASE + 'get', {'id': value['id']}, expected=404)
    # Первая услуга не должна остаться привязанной после ошибки второй.
    saved = await api.post(BASE + 'save', invoice([first['id']]))
    assert saved['charges'] == [first]
