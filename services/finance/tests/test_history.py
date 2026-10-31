"""История: снимки полей, связи в обе стороны, пагинация и границы доступа."""
from uuid import uuid4
import pytest
from test_invoices import invoice, saved_entry, invoice_references
from test_payments import payment, allocation, billed
from test_service_charges import references, scope, charge

async def history(api, kind, id, **changes):
    return await api.post(f'/api/v1/{kind}/history', dict(id=id, limit=50, **changes))

def fields(event):
    return {item['field']: item for item in event['changes']}

async def test_service_charge_field_values_and_actor_are_snapshots(api, charge):
    body, item = await saved_entry(api, charge)
    await api.post('/api/v1/service-charges/save', {**body, 'expected_revision':1, 'price_minor':12345, 'note':'Уточнён тариф'})
    result = await history(api, 'service-charges', item['id'])
    assert [e['cursor']['revision'] for e in result['items']] == [2, 1]
    event = result['items'][0]
    assert event['has_previous'] is True
    assert event['actor'].endswith('#author')
    assert event['actor_name'] == 'author'
    price = fields(event)['price_minor']
    assert price['before'] == dict(value='10000', format='money', currency='RUB')
    assert price['after'] == dict(value='12345', format='money', currency='RUB')
    assert fields(event)['note']['after']['value'] == 'Уточнён тариф'
    assert 'note' not in fields(result['items'][1])

async def test_service_assignment_and_removal_are_visible_from_service(api, charge, invoice_references):
    _, item = await saved_entry(api, charge)
    document = invoice([item['id']])
    await api.post('/api/v1/invoices/save', document)
    included = (await history(api, 'service-charges', item['id']))['items']
    assert included[0]['cursor']['source'] == 'invoice'
    assert fields(included[0])['invoice']['after']['value'] == '№ ИС-1'
    await api.post('/api/v1/invoices/detach-service-charge', dict(invoice_id=document['id'], service_charge_id=item['id']))
    events = (await history(api, 'service-charges', item['id']))['items']
    assert events[0]['title'].startswith('Услуга исключена')
    assert 'after' not in fields(events[0])['invoice']
    assert fields(events[0])['invoice']['before']['value'] == '№ ИС-1'
    own_only = await api.post('/api/v1/service-charges/history', dict(id=item['id'],limit=50), roles=['finance.read_service_charges','shipments.read','transportations.read','counterparties.read','organizations.read_employees'])
    assert [e['cursor']['source'] for e in own_only['items']] == ['service_charge']

async def test_invoice_lines_have_own_before_after_values(api, charge, invoice_references):
    body, item = await saved_entry(api, charge)
    document=invoice([item['id']])
    await api.post('/api/v1/invoices/save', document)
    await api.post('/api/v1/service-charges/save', {**body,'expected_revision':1,'price_minor':20000})
    events=(await history(api,'invoices',document['id']))['items']
    changed=fields(events[0])
    assert changed['total_minor']['before']['value']=='12000'
    assert changed['total_minor']['after']['value']=='24000'
    assert changed[f"charges.{item['id']}.price_minor"]['after']['value']=='20000'
    assert fields(events[1])[f"charges.{item['id']}"]['after']['value']=='Включена в счёт'

@pytest.mark.parametrize('direction,kind', [('outgoing','receipt'),('incoming','disbursement')])
async def test_payment_allocation_changes_and_archive_appear_in_invoice_history(api, charge, invoice_references, direction, kind):
    _, item = await saved_entry(api, charge, direction='income' if direction=='outgoing' else 'expense')
    document = {**invoice([item['id']]), 'direction':direction}
    await api.post('/api/v1/invoices/save', document)
    body=payment([allocation(document['id'],5000)],kind=kind)
    await api.post('/api/v1/payments/save',body)
    first=(await history(api,'invoices',document['id']))['items'][0]
    assert first['cursor']['source']=='payment'
    assert fields(first)['allocation']['after']['value']=='5000'
    await api.post('/api/v1/payments/save',{**body,'expected_revision':1,'allocations':[allocation(document['id'],8000)]})
    await api.post('/api/v1/payments/save',{**body,'expected_revision':2,'archived':True,'allocations':[allocation(document['id'],8000)]})
    events=(await history(api,'invoices',document['id']))['items']
    archived=fields(events[0])['allocation']
    assert archived['before']['value']=='8000' and 'after' not in archived
    changed=fields(events[1])['allocation']
    assert changed['before']['value']=='5000' and changed['after']['value']=='8000'
    payments=(await history(api,'payments',body['id']))['items']
    assert fields(payments[0])[f"allocations.{document['id']}"]['before']['value']=='8000'
    assert any('ИС-1' in c['label'] for c in payments[0]['changes'])
    own_only=await api.post('/api/v1/invoices/history',dict(id=document['id'],limit=50),roles=['finance.read_invoices','finance.read_service_charges','shipments.read','transportations.read','counterparties.read','organizations.read_employees'])
    assert [e['cursor']['source'] for e in own_only['items']]==['invoice']

async def test_changes_to_other_allocations_do_not_pollute_invoice_history(api, charge, invoice_references):
    first,_,_=await billed(api,charge)
    second,_,_=await billed(api,charge)
    body=payment([allocation(first['id'],1000),allocation(second['id'],1000)])
    await api.post('/api/v1/payments/save',body)
    await api.post('/api/v1/payments/save',{**body,'expected_revision':1,'allocations':[allocation(first['id'],1000),allocation(second['id'],2000)]})
    events=(await history(api,'invoices',first['id']))['items']
    assert [(e['cursor']['source'],e['cursor']['revision']) for e in events]==[('payment',1),('invoice',1)]

async def test_composite_cursor_does_not_lose_equal_time_events(api, charge, invoice_references, pgsql):
    document,body,_=await billed(api,charge)
    for revision in range(1,4):
        await api.post('/api/v1/service-charges/save',{**body,'expected_revision':revision,'price_minor':10000+revision})
    with pgsql['db_1'].cursor() as c:
        c.execute("UPDATE service_charge_history SET occurred_at = '2026-10-30T12:00:00Z'")
        c.execute("UPDATE invoice_history SET occurred_at = '2026-10-30T12:00:00Z'")
    expected=(await history(api,'invoices',document['id']))['items']
    query=dict(id=document['id'],limit=1)
    collected=[]
    while True:
        page=await api.post('/api/v1/invoices/history',query)
        collected.extend(page['items'])
        if not page.get('next_before'): break
        query['before']=page['next_before']
    assert collected==expected
    assert len(collected)==4

@pytest.mark.parametrize('kind', ['service-charges','invoices','payments'])
async def test_history_permission_and_organization_boundaries(api, charge, invoice_references, kind):
    if kind=='service-charges': _, item=await saved_entry(api,charge); id=item['id']
    elif kind=='invoices': body=invoice([]); await api.post('/api/v1/invoices/save',body); id=body['id']
    else: body=payment(); await api.post('/api/v1/payments/save',body); id=body['id']
    query=dict(id=id,limit=50)
    await api.post(f'/api/v1/{kind}/history',query,user='other',expected=404)
    await api.post(f'/api/v1/{kind}/history',query,roles=[],expected=403)
    await api.post(f'/api/v1/{kind}/history',{**query,'id':str(uuid4())},expected=404)

async def test_legacy_missing_previous_snapshot_is_not_invented(api, charge, pgsql):
    body,item=await saved_entry(api,charge)
    await api.post('/api/v1/service-charges/save',{**body,'expected_revision':1,'note':'Изменено'})
    with pgsql['db_1'].cursor() as c:
        c.execute('DELETE FROM service_charge_history WHERE service_charge_id=%s::UUID AND revision=1',(item['id'],))
        c.execute("UPDATE service_charge_history SET snapshot = snapshot - 'actor_name' - 'invoice_rate' WHERE service_charge_id=%s::UUID",(item['id'],))
    event=(await history(api,'service-charges',item['id']))['items'][0]
    assert not event['has_previous']
    assert 'actor_name' not in event
    assert 'before' not in fields(event)['note']

async def test_failed_mutation_does_not_add_history(api, charge):
    body,item=await saved_entry(api,charge)
    before=await history(api,'service-charges',item['id'])
    await api.post('/api/v1/service-charges/save',{**body,'expected_revision':9},expected=409)
    assert await history(api,'service-charges',item['id'])==before

async def test_late_validation_rollback_does_not_leave_phantom_history(api, charge, invoice_references):
    document,body,_=await billed(api,charge)
    await api.post('/api/v1/payments/save',payment([allocation(document['id'],1000)]))
    before=await history(api,'service-charges',body['id'])
    await api.post('/api/v1/service-charges/save',{**body,'expected_revision':1,'price_minor':20000},expected=409)
    assert await history(api,'service-charges',body['id'])==before
