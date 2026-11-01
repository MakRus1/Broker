"""Акт сверки: остатки по датам документов и авансы отдельно от распределений."""
import pytest
from test_service_charges import references, scope, charge, PARTY, CONTRACT
from test_invoices import invoice_references
from test_settlements import create_invoice
from test_payments import payment, allocation

BASE = '/api/v1/counterparty-reconciliation/get'
PERIOD = dict(counterparty_id=PARTY, date_from='2026-10-01', date_to='2026-10-31')


async def test_opening_turnover_and_advances(api, charge, invoice_references):
    await create_invoice(api, charge, invoice_date='2026-09-30')
    await create_invoice(api, charge, invoice_date='2026-10-01')
    await create_invoice(api, charge, direction='incoming', invoice_date='2026-10-31')
    await create_invoice(api, charge, invoice_date='2026-11-01')
    await api.post('/api/v1/payments/save', payment(amount_minor=3000, payment_date='2026-09-30'))
    await api.post('/api/v1/payments/save', payment(amount_minor=5000, payment_date='2026-10-15'))
    result = await api.post(BASE, PERIOD)
    assert result['totals'] == [dict(currency='RUB', opening_minor='9000', debit_minor='12000', credit_minor='17000', closing_minor='4000')]
    assert len(result['operations']) == 3
    assert [item['balance_minor'] for item in result['operations']] == ['21000', '16000', '4000']
    assert (await api.post(BASE, PERIOD, user='other')) == dict(totals=[], operations=[])


async def test_contract_counts_allocations_only(api, charge, invoice_references):
    document, _ = await create_invoice(api, charge, invoice_date='2026-10-01')
    await api.post('/api/v1/payments/save', payment([allocation(document['id'], 2000)], amount_minor=5000, payment_date='2026-10-15'))
    result = await api.post(BASE, {**PERIOD, 'contract_id': CONTRACT})
    assert result['totals'][0]['credit_minor'] == '2000'
    assert result['totals'][0]['closing_minor'] == '10000'
    assert (await api.post(BASE, {**PERIOD, 'date_from':'2026-11-01', 'date_to':'2026-11-30'}))['operations'] == []


@pytest.mark.parametrize('roles', [[], ['finance.read_invoices'], ['finance.read_payments']])
async def test_requires_both_permissions(api, roles):
    await api.post(BASE, PERIOD, roles=roles, expected=403)


async def test_invalid_period(api):
    await api.post(BASE, {**PERIOD, 'date_from':'2026-11-01'}, expected=400)


async def test_currency_and_archive_are_independent(api, invoice_references):
    body = payment(amount_minor=300, currency='USD', payment_date='2026-10-31')
    await api.post('/api/v1/payments/save', body)
    await api.post('/api/v1/payments/save', payment(amount_minor=700, payment_date='2026-09-30'))
    result = await api.post(BASE, PERIOD)
    assert result['totals'] == [
        dict(currency='RUB', opening_minor='-700', debit_minor='0', credit_minor='0', closing_minor='-700'),
        dict(currency='USD', opening_minor='0', debit_minor='0', credit_minor='300', closing_minor='-300'),
    ]
    await api.post('/api/v1/payments/save', {**body, 'expected_revision': 1, 'archived': True})
    assert (await api.post(BASE, {**PERIOD, 'currency': 'USD'})) == dict(totals=[], operations=[])
