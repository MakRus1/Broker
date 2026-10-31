"""Задолженность считается по активным оплатам; просрочка — по календарному сроку."""
from datetime import timedelta

import pytest

from test_invoices import invoice, saved_entry, invoice_references
from test_payments import allocation, payment
from test_service_charges import charge, references, scope


@pytest.mark.parametrize('direction', ['outgoing', 'incoming'])
async def test_debt_status_overdue_filters_and_numeric_sort(api, charge, invoice_references, pgsql, direction):
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT CURRENT_DATE')
        today = cursor.fetchone()[0]

    async def document(number, due, total, paid=0, archived=False):
        _, service = await saved_entry(api, charge,
            direction='income' if direction == 'outgoing' else 'expense',
            quantity_milli=1000, price_minor=total, vat_rate_basis_points=0)
        body = {**invoice([service['id']]), 'direction': direction, 'number': number,
                'due_date': due.isoformat() if due else None, 'archived': archived}
        await api.post('/api/v1/invoices/save', body)
        paid_body = None
        if paid:
            paid_body = payment([allocation(body['id'], paid)], amount_minor=paid,
                kind='receipt' if direction == 'outgoing' else 'disbursement')
            await api.post('/api/v1/payments/save', paid_body)
        return body, paid_body

    late, _ = await document('Д-1', today-timedelta(days=3), 100)
    partial, _ = await document('Д-2', today-timedelta(days=5), 20000, 10000)
    paid, paid_body = await document('Д-3', today-timedelta(days=10), 900, 900)
    no_due, _ = await document('Д-4', None, 200)
    due_today, _ = await document('Д-5', today, 300)
    future, _ = await document('Д-6', today+timedelta(days=1), 400)
    archived, _ = await document('Д-7', today-timedelta(days=20), 500, archived=True)

    query = {'direction': direction, 'limit': 100}
    rows = (await api.post('/api/v1/invoices/list', query))['items']
    by_id = {row['invoice']['id']: row for row in rows}
    assert by_id[late['id']]['overdue_days'] == 3
    assert by_id[partial['id']]['overdue_days'] == 5
    assert by_id[partial['id']]['payment_status'] == 'partial'
    assert by_id[partial['id']]['paid_minor'] == 10000
    assert by_id[partial['id']]['outstanding_minor'] == 10000
    assert by_id[paid['id']]['payment_status'] == 'paid'
    assert by_id[paid['id']]['outstanding_minor'] == 0
    for document_id in [paid['id'], no_due['id'], due_today['id'], future['id'], archived['id']]:
        assert by_id[document_id]['overdue_days'] == 0

    filtered = await api.post('/api/v1/invoices/list', {**query, 'filters': [
        {'field': 'overdue', 'values': ['true']},
        {'field': 'payment_status', 'values': ['partial']}]})
    assert [row['invoice']['id'] for row in filtered['items']] == [partial['id']]
    facets = await api.post('/api/v1/invoices/filter-values', {
        'registry': query, 'field': 'payment_status'})
    assert {item['label'] for item in facets['values']} == {'Не оплачен', 'Частично оплачен', 'Оплачен'}
    overdue_values = await api.post('/api/v1/invoices/filter-values', {
        'registry': query, 'field': 'overdue'})
    assert {item['value'] for item in overdue_values['values']} == {'true', 'false'}

    # Numeric cursor must not turn 100, 200, 10000 into lexicographic order.
    for descending in [False, True]:
        values, cursor = [], None
        while True:
            page = await api.post('/api/v1/invoices/list', {
                **query, 'sort': 'outstanding_minor', 'descending': descending, 'limit': 2,
                **({'cursor': cursor} if cursor else {})})
            values.extend(row['outstanding_minor'] for row in page['items'])
            cursor = page.get('next_cursor')
            if not cursor:
                break
        assert values == sorted([100, 10000, 0, 200, 300, 400, 500], reverse=descending)

    first = await api.post('/api/v1/invoices/list', {**query, 'sort': 'outstanding_minor', 'limit': 2})
    await api.post('/api/v1/invoices/list', {**query, 'sort': 'outstanding_minor', 'limit': 2,
        'cursor': {**first['next_cursor'], 'text_value': 'not-a-number'}}, expected=400)

    await api.post('/api/v1/payments/save', {**paid_body, 'expected_revision': 1, 'archived': True})
    reopened = await api.post('/api/v1/invoices/list', {**query, 'filters': [
        {'field': 'number', 'values': [paid['number']]}]})
    assert reopened['items'][0]['payment_status'] == 'unpaid'
    assert reopened['items'][0]['overdue_days'] == 10
