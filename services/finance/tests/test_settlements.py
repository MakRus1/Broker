"""Взаиморасчёты: суммы по счетам, распределения и независимые валюты."""
from uuid import uuid4

import pytest
from test_service_charges import references, scope, charge, PARTY, CONTRACT, SHIPMENT
from test_invoices import invoice_references, invoice, saved_entry
from test_payments import payment, allocation

BASE = '/api/v1/counterparty-settlements/get'


async def create_invoice(api, charge, *, direction='outgoing', price=10000, **changes):
    _, item = await saved_entry(api, charge, direction='income' if direction == 'outgoing' else 'expense', price_minor=price)
    body = {**invoice([item['id']]), 'direction': direction,
            'invoice_date': '2000-01-01', 'due_date': '2000-01-02', **changes}
    return body, (await api.post('/api/v1/invoices/save', body))['invoice']


async def test_balances_do_not_double_count_services_or_allocations(api, charge, invoice_references):
    first, _ = await create_invoice(api, charge)
    second, _ = await create_invoice(api, charge)
    incoming, _ = await create_invoice(api, charge, direction='incoming', price=5000)
    await saved_entry(api, charge, price_minor=999999)  # Ещё не выставленная услуга не является долгом.
    receipt = payment([allocation(first['id'], 5000), allocation(second['id'], 3000)], amount_minor=9000)
    await api.post('/api/v1/payments/save', receipt)
    await api.post('/api/v1/payments/save', payment([
        allocation(incoming['id'], 2000)], kind='disbursement', amount_minor=3000))
    result = await api.post(BASE, {'counterparty_id': PARTY})
    assert result['totals'] == [{
        'currency': 'RUB', 'receivable_minor': '16000', 'payable_minor': '4000',
        'overdue_receivable_minor': '16000', 'overdue_payable_minor': '4000',
        'unallocated_receipts_minor': '1000', 'unallocated_disbursements_minor': '1000',
    }]
    available = await api.post('/api/v1/payments/list', {
        'kind': 'receipt', 'filters': [{'field': 'has_unallocated', 'values': ['true']}], 'limit': 50})
    assert [item['id'] for item in available['items']] == [receipt['id']]
    for document in [first, second, incoming]:
        balance = await api.post('/api/v1/payments/invoice-balance', {'id': document['id']})
        assert balance['outstanding_minor'] == (4000 if document is incoming else 7000 if document is first else 9000)


async def test_filters_keep_unallocated_money_separate_from_contract(api, charge, invoice_references):
    await create_invoice(api, charge)
    await api.post('/api/v1/payments/save', payment(amount_minor=700))
    await api.post('/api/v1/payments/save', payment(amount_minor=300, currency='USD'))
    result = await api.post(BASE, {'counterparty_id': PARTY})
    assert [row['currency'] for row in result['totals']] == ['RUB', 'USD']
    rub = (await api.post(BASE, {'counterparty_id': PARTY, 'currency': 'RUB', 'own_company_id': SHIPMENT}))['totals'][0]
    assert rub['receivable_minor'] == '12000' and rub['unallocated_receipts_minor'] == '700'
    filtered = (await api.post(BASE, {'counterparty_id': PARTY, 'contract_id': str(uuid4()), 'currency': 'RUB'}))['totals'][0]
    assert filtered['receivable_minor'] == '0'
    assert filtered['unallocated_receipts_minor'] == '700'
    assert (await api.post(BASE, {'counterparty_id': PARTY, 'own_company_id': str(uuid4())}))['totals'] == []
    assert (await api.post(BASE, {'counterparty_id': str(uuid4())}))['totals'] == []
    assert (await api.post(BASE, {'counterparty_id': PARTY}, user='other'))['totals'] == []


async def test_archived_documents_and_payments_are_excluded(api, charge, invoice_references):
    first, _ = await create_invoice(api, charge)
    unused, _ = await create_invoice(api, charge)
    await api.post('/api/v1/invoices/save', {**unused, 'expected_revision': 1, 'archived': True})
    body = payment([allocation(first['id'], 4000)], amount_minor=5000)
    await api.post('/api/v1/payments/save', body)
    await api.post('/api/v1/payments/save', {**body, 'expected_revision': 1, 'archived': True})
    total = (await api.post(BASE, {'counterparty_id': PARTY}))['totals'][0]
    assert total['receivable_minor'] == '12000'
    assert total['overdue_receivable_minor'] == '12000'
    assert total['unallocated_receipts_minor'] == '0'


async def test_future_due_date_and_paid_invoice_are_not_overdue(api, charge, invoice_references):
    document, _ = await create_invoice(api, charge)
    await api.post('/api/v1/payments/save', payment([allocation(document['id'], 12000)], amount_minor=12000))
    await create_invoice(api, charge, due_date='2099-01-01')
    total = (await api.post(BASE, {'counterparty_id': PARTY}))['totals'][0]
    assert total['receivable_minor'] == '12000'
    assert total['overdue_receivable_minor'] == '0'
    assert (await api.post('/api/v1/payments/list', {'kind': 'receipt', 'filters': [{'field': 'has_unallocated', 'values': ['true']}]}))['items'] == []


@pytest.mark.parametrize('roles', [[], ['finance.read_invoices'], ['finance.read_payments']])
async def test_both_finance_permissions_are_required(api, roles):
    await api.post(BASE, {'counterparty_id': PARTY}, roles=roles, expected=403)
