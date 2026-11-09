"""История: снимки полей, связи в обе стороны, пагинация и границы доступа."""
from uuid import uuid4
import pytest
from test_invoices import invoice, saved_entry, invoice_references
from test_payments import payment, allocation, billed
from test_service_charges import references, scope, charge

async def history(api, kind, id, **changes):
    return await api.post(f'/api/v1/{kind}/history', dict(id=id, limit=50, **changes))

def fields(event):
    result = {}
    for item in event['changes']:
        subject = item.get('subject')
        key = item['field']
        if subject:
            if subject['source'] == 'service_charge':
                key = f"charges.{subject['id']}" + (f".{key}" if key != 'charges' else '')
            elif subject['source'] == 'invoice':
                key = f"allocations.{subject['id']}"
        result[key] = item
    return result

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
    assert included[0]['action'] == 'invoice_linked'
    assert included[0]['reference'] == dict(source='invoice', id=document['id'], name='ИС-1')
    assert fields(included[0])['invoice']['after'] == dict(value=document['id'], display_name='ИС-1', format='reference')
    await api.post('/api/v1/invoices/detach-service-charge', dict(invoice_id=document['id'], service_charge_id=item['id']))
    events = (await history(api, 'service-charges', item['id']))['items']
    assert events[0]['action'] == 'invoice_unlinked'
    assert 'after' not in fields(events[0])['invoice']
    assert fields(events[0])['invoice']['before']['value'] == document['id']
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
    assert fields(events[1])[f"charges.{item['id']}"]['after']==dict(value='true',format='boolean')

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
    assert any(c.get('subject') == dict(source='invoice', id=document['id'], name='ИС-1') for c in payments[0]['changes'])
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
    assert event['action'] == 'updated'
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

async def test_history_has_codes_instead_of_localized_descriptions(api, charge):
    body, item = await saved_entry(api, charge)
    await api.post('/api/v1/service-charges/save', {**body, 'expected_revision': 1,
        'direction': 'expense', 'unit': 'ton', 'price_mode': 'gross', 'archived': True})
    events = (await history(api, 'service-charges', item['id']))['items']
    changed = fields(events[0])
    assert events[0]['action'] == 'archived'
    assert events[0]['document'] == 'service_charge'
    assert events[1]['action'] == 'created'
    assert changed['direction']['before'] == dict(format='enum', value='income')
    assert changed['direction']['after'] == dict(format='enum', value='expense')
    assert changed['unit']['after'] == dict(format='enum', value='ton')
    assert changed['price_mode']['after'] == dict(format='enum', value='gross')
    assert changed['archived']['after'] == dict(format='boolean', value='true')
    for event in events:
        assert 'title' not in event
        assert all('label' not in change for change in event['changes'])

@pytest.mark.parametrize('kind,document', [('service-charges','service_charge'),('invoices','invoice'),('payments','receipt'),('payments','disbursement')])
async def test_document_lifecycle_is_structured(api, charge, invoice_references, kind, document):
    if kind == 'service-charges':
        body, item = await saved_entry(api, charge)
        id = item['id']
    else:
        body = invoice([]) if kind == 'invoices' else payment(kind=document)
        await api.post(f'/api/v1/{kind}/save', body)
        id = body['id']
    await api.post(f'/api/v1/{kind}/save', {**body, 'expected_revision': 1, 'archived': True})
    await api.post(f'/api/v1/{kind}/save', {**body, 'expected_revision': 2, 'archived': False})
    events = (await history(api, kind, id))['items']
    assert [event['action'] for event in events] == ['restored', 'archived', 'created']
    assert {event['document'] for event in events} == {document}

async def test_missing_legacy_fields_are_not_filled_with_current_defaults(api, charge, pgsql):
    body, item = await saved_entry(api, charge)
    await api.post('/api/v1/service-charges/save', {**body, 'expected_revision': 1,
        'note': 'Дополнены условия'})
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute("""
            UPDATE service_charge_history
            SET snapshot = snapshot - 'invoice_rate' - 'service_date' - 'price_mode'
            WHERE service_charge_id = %s::UUID AND revision = 1
        """, (item['id'],))
    events = (await history(api, 'service-charges', item['id']))['items']
    changed = fields(events[0])
    assert events[0]['has_previous']
    assert changed['invoice_rate.rate_millionths']['after'] == dict(format='rate', value='1000000')
    assert 'before' not in changed['invoice_rate.rate_millionths']
    assert 'before' not in changed['service_date']
    assert 'before' not in changed['price_mode']
    assert not {'invoice_rate.rate_millionths', 'invoice_rate.date', 'invoice_rate.source',
        'service_date', 'price_mode'} & fields(events[1]).keys()

async def test_reference_name_change_is_visible_without_changing_id(api, charge, pgsql):
    body, item = await saved_entry(api, charge)
    await api.post('/api/v1/service-charges/save', {**body, 'expected_revision': 1,
        'note': 'Уточнение'})
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute("""
            UPDATE service_charge_history
            SET snapshot = jsonb_set(snapshot, '{counterparty_name}', to_jsonb(%s::TEXT))
            WHERE service_charge_id = %s::UUID AND revision = 1
        """, ('Предыдущее название', item['id']))
    event = (await history(api, 'service-charges', item['id']))['items'][0]
    change = fields(event)['counterparty_id']
    assert change['before']['value'] == change['after']['value'] == body['counterparty_id']
    assert change['before']['display_name'] == 'Предыдущее название'
    assert change['after']['display_name'] != 'Предыдущее название'
