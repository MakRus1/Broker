"""Платежи: атомарное распределение, конкурентная оплата и защита оплаченных счетов."""
import asyncio
from uuid import uuid4
import pytest
from test_invoices import invoice, saved_entry, invoice_references
from test_service_charges import references, scope, charge, PARTY, SHIPMENT
BASE = '/api/v1/payments/'

def payment(allocations=(), **changes):
    return {**dict(id=str(uuid4()),kind='receipt',number='ПП-1',payment_date='2026-10-30',
        own_company_id=SHIPMENT,counterparty_id=PARTY,own_bank_account_id=None,
        counterparty_bank_account_id=None,currency='RUB',amount_minor=20000,purpose='Оплата услуг',
        note=None,archived=False,expected_revision=0,allocations=list(allocations)),**changes}

def allocation(id, amount): return {'invoice_id':id,'amount_minor':amount}

async def billed(api, charge, **changes):
    body,item=await saved_entry(api,charge)
    document={**invoice([item['id']]),**changes}
    result=await api.post('/api/v1/invoices/save',document)
    return document,body,result['invoice']

async def test_payment_without_distribution_and_revision(api, invoice_references):
    body=payment()
    result=await api.post(BASE+'save',body)
    assert result['allocated_minor']==0 and result['unallocated_minor']==20000
    assert await api.post(BASE+'get',{'id':body['id']})==result
    await api.post(BASE+'save',body,expected=409)
    await api.post(BASE+'save',{**body,'expected_revision':1,'amount_minor':10000})
    await api.post(BASE+'save',{**body,'expected_revision':1},expected=409)

async def test_one_payment_many_invoices_and_partial_payments(api, charge, invoice_references):
    first,_,_=await billed(api,charge)
    second,_,_=await billed(api,charge)
    body=payment([allocation(first['id'],10000),allocation(second['id'],5000)])
    result=await api.post(BASE+'save',body)
    assert result['allocated_minor']==15000 and result['unallocated_minor']==5000
    balance=await api.post(BASE+'invoice-balance',{'id':first['id']})
    assert balance['paid_minor']==10000 and balance['outstanding_minor']==2000
    await api.post(BASE+'save',payment([allocation(first['id'],2000)],amount_minor=2000))
    assert (await api.post(BASE+'invoice-balance',{'id':first['id']}))['outstanding_minor']==0
    await api.post(BASE+'save',payment([allocation(first['id'],1)]),expected=409)

async def test_invalid_distribution_rolls_back_payment_and_allocations(api, charge, invoice_references):
    document,_,_=await billed(api,charge)
    body=payment([allocation(document['id'],5000)])
    before=await api.post(BASE+'save',body)
    for changes in [dict(allocations=[allocation(document['id'],20001)]),
                    dict(allocations=[allocation(document['id'],1000),allocation(document['id'],1000)]),
                    dict(currency='USD'),dict(kind='disbursement')]:
        await api.post(BASE+'save',{**body,'expected_revision':1,**changes},expected=(400,409))
        assert await api.post(BASE+'get',{'id':body['id']})==before

async def test_archive_releases_payment_and_restore_checks_balance(api, charge, invoice_references):
    document,_,_=await billed(api,charge)
    body=payment([allocation(document['id'],12000)])
    await api.post(BASE+'save',body)
    await api.post(BASE+'save',{**body,'expected_revision':1,'archived':True})
    assert (await api.post(BASE+'invoice-balance',{'id':document['id']}))['paid_minor']==0
    await api.post(BASE+'save',payment([allocation(document['id'],12000)]))
    await api.post(BASE+'save',{**body,'expected_revision':2},expected=409)

async def test_competing_payments_cannot_overpay(api, charge, invoice_references):
    document,_,_=await billed(api,charge)
    async def save(): return await api.post(BASE+'save',payment([allocation(document['id'],10000)]),expected=(200,409))
    results=await asyncio.gather(save(),save())
    assert sum('payment' in r for r in results)==1
    assert (await api.post(BASE+'invoice-balance',{'id':document['id']}))['paid_minor']==10000

async def test_paid_invoice_cannot_be_reduced_archived_or_change_parties(api, charge, invoice_references):
    document,service,value=await billed(api,charge)
    await api.post(BASE+'save',payment([allocation(document['id'],12000)]))
    await api.post('/api/v1/invoices/save',{**document,'expected_revision':1,'archived':True},expected=409)
    await api.post('/api/v1/invoices/detach-service-charge',{'invoice_id':document['id'],'service_charge_id':service['id']},expected=409)
    await api.post('/api/v1/service-charges/save',{**service,'expected_revision':1,'price_minor':1},expected=409)
    assert (await api.post('/api/v1/invoices/get',{'id':document['id']}))['invoice']==value

async def test_candidates_registry_and_permissions(api, charge, invoice_references):
    document,_,_=await billed(api,charge)
    candidates=await api.post(BASE+'invoice-candidates',dict(kind='receipt',own_company_id=SHIPMENT,counterparty_id=PARTY,currency='RUB',limit=40))
    assert [r['invoice']['id'] for r in candidates['items']]==[document['id']]
    for n in ['П-1','П-10','П-2']: await api.post(BASE+'save',payment(number=n))
    request=dict(kind='receipt',sort='number',limit=1)
    result=await api.post(BASE+'list',request)
    assert result['items'][0]['number']=='П-1'
    result=await api.post(BASE+'list',{**request,'cursor':result['next_cursor']})
    assert result['items'][0]['number']=='П-2'
    facets=await api.post(BASE+'filter-values',{'registry':{'kind':'receipt'},'field':'number'})
    assert len(facets['values'])==3
    await api.post(BASE+'save',payment(),roles=['finance.read_payments'],expected=403)
    await api.post(BASE+'list',{'kind':'receipt'},roles=[],expected=403)

async def test_partial_payment_locks_invoice_and_services_until_unallocated(api, charge, invoice_references, scope):
    document, service, value = await billed(api, charge)
    body = payment([allocation(document['id'], 100)])
    await api.post(BASE+'save', body)
    for changes in [dict(number='Изменённый'), dict(note='Изменение')]:
        error=await api.post('/api/v1/invoices/save',{**document,'expected_revision':1,**changes},expected=409)
        assert error['error']=='invoice_paid_readonly'
    await api.post('/api/v1/service-charges/save',{**service,'expected_revision':1,'price_minor':20000},expected=409)
    rows=await api.post('/api/v1/service-charges/list',dict(scope=scope,after=None,limit=50,include_archived=False))
    assert rows['invoices'][0]['read_only'] is True
    await api.post(BASE+'save',{**body,'expected_revision':1,'allocations':[]})
    await api.post('/api/v1/invoices/save',{**document,'expected_revision':1,'number':'Исправленный'})

async def test_payment_selection_from_invoice_uses_free_balance(api, charge, invoice_references):
    document,_,_=await billed(api,charge)
    body=payment(amount_minor=20000)
    await api.post(BASE+'save',body)
    result=await api.post(BASE+'payment-candidates',{'invoice_id':document['id']})
    assert [(item['payment']['id'],item['unallocated_minor']) for item in result['items']]==[(body['id'],20000)]
    await api.post(BASE+'save',{**body,'expected_revision':1,'allocations':[allocation(document['id'],1000)]})
    balance=await api.post(BASE+'invoice-balance',{'id':document['id']})
    assert balance['payments'][0]['payment']['id']==body['id']
    assert balance['payments'][0]['amount_minor']==1000
    result=await api.post(BASE+'payment-candidates',{'invoice_id':document['id']})
    assert result['items'][0]['unallocated_minor']==19000

async def test_disbursement_pays_only_incoming_invoices(api, charge, invoice_references):
    _, expense = await saved_entry(api, charge, direction='expense')
    incoming = {**invoice([expense['id']]), 'direction': 'incoming', 'number': 'ВС-1'}
    await api.post('/api/v1/invoices/save', incoming)
    outgoing, _, _ = await billed(api, charge)
    body = payment(kind='disbursement', number='СП-1')
    receipt = payment(number='ПП-2')
    await api.post(BASE+'save', body)
    await api.post(BASE+'save', receipt)
    candidates = await api.post(BASE+'invoice-candidates', dict(
        kind='disbursement', own_company_id=SHIPMENT, counterparty_id=PARTY, currency='RUB', limit=40))
    assert [item['invoice']['id'] for item in candidates['items']] == [incoming['id']]
    available = await api.post(BASE+'payment-candidates', {'invoice_id': incoming['id']})
    assert [item['payment']['id'] for item in available['items']] == [body['id']]
    await api.post(BASE+'save', {**body, 'expected_revision': 1,
        'allocations': [allocation(outgoing['id'], 100)]}, expected=400)
    await api.post(BASE+'save', {**body, 'expected_revision': 1,
        'allocations': [allocation(incoming['id'], 100)]})
    balance = await api.post(BASE+'invoice-balance', {'id': incoming['id']})
    assert balance['paid_minor'] == 100
    assert balance['payments'][0]['payment']['kind'] == 'disbursement'
    await api.post('/api/v1/invoices/save', {**incoming, 'expected_revision': 1}, expected=409)
    for kind, document in [('receipt', receipt), ('disbursement', body)]:
        rows = await api.post(BASE+'list', {'kind': kind})
        assert [item['id'] for item in rows['items']] == [document['id']]
    await api.post(BASE+'save', {**body, 'expected_revision': 2, 'archived': True,
        'allocations': [allocation(incoming['id'], 100)]})
    balance = await api.post(BASE+'invoice-balance', {'id': incoming['id']})
    assert balance['paid_minor'] == 0
