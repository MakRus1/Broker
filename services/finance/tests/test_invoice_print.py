"""Печатная форма фиксирует реквизиты, но не теряет контроль редакции счёта."""
import pytest
from test_invoices import invoice, invoice_references, saved_entry, charge, scope, references

BASE = '/api/v1/invoices/'

async def test_print_snapshot_survives_reference_changes(api, charge, invoice_references, mockserver):
    _, item = await saved_entry(api, charge)
    body = invoice([item['id']])
    saved = await api.post(BASE + 'save', body)
    query = dict(id=body['id'], revision=1)
    printed = await api.post(BASE + 'print-data', query)
    assert printed['invoice'] == saved['invoice']
    assert printed['charges'] == saved['charges']
    assert printed['supplier']['name'] == 'Своя компания'
    assert printed['customer']['name'] == 'Клиент'
    assert 'supplier_bank' not in printed

    @mockserver.json_handler('/api/v1/counterparties/get')
    def unavailable(request):
        return mockserver.make_response(status=503)

    assert await api.post(BASE + 'print-data', query) == printed
    await api.post(BASE + 'print-data', query, roles=[], expected=403)
    await api.post(BASE + 'print-data', query, user='other', expected=404)
    await api.post(BASE + 'print-data', {**query, 'revision': 2}, expected=409)

async def test_new_invoice_revision_requires_new_requisites(api, charge, invoice_references, mockserver):
    _, item = await saved_entry(api, charge)
    body = invoice([item['id']])
    await api.post(BASE + 'save', body)
    await api.post(BASE + 'print-data', dict(id=body['id'], revision=1))
    await api.post(BASE + 'save', {**body, 'expected_revision': 1, 'note': 'Новая редакция'})
    await api.post(BASE + 'print-data', dict(id=body['id'], revision=1), expected=409)
    printed = await api.post(BASE + 'print-data', dict(id=body['id'], revision=2))
    assert printed['invoice']['note'] == 'Новая редакция'
    assert printed['invoice']['revision'] == 2

async def test_incoming_invoice_has_no_outgoing_print_form(api, invoice_references):
    body = {**invoice([]), 'direction': 'incoming'}
    await api.post(BASE + 'save', body)
    await api.post(BASE + 'print-data', dict(id=body['id'], revision=1), expected=400)

async def test_print_captures_selected_bank_and_retries_failed_lookup(api, invoice_references, mockserver):
    from uuid import uuid4
    from test_service_charges import SHIPMENT
    bank_id = str(uuid4())
    bank = dict(id=bank_id, counterparty_id=SHIPMENT, name='Основной', number='40702810000000000001',
                currency='RUB', bank_name='Банк', bic='044525225', swift=None,
                correspondent_account='30101810400000000225', iban=None, is_primary=True, archived=False, revision=1)
    state = {'status': 200}

    @mockserver.json_handler('/api/v1/counterparties/bank-accounts/list')
    def accounts(request):
        if state['status'] != 200:
            return mockserver.make_response(status=state['status'], json={'error':'unavailable','message':'Недоступно'})
        return {'items': [bank]}

    body = {**invoice([]), 'own_bank_account_id': bank_id}
    await api.post(BASE + 'save', body)
    query = dict(id=body['id'], revision=1)
    state['status'] = 503
    await api.post(BASE + 'print-data', query, expected=502)
    state['status'] = 200
    printed = await api.post(BASE + 'print-data', query)
    assert printed['supplier_bank']['number'] == bank['number']
    assert printed['supplier_bank']['bic'] == bank['bic']
    bank['number'] = '40702810000000000002'
    assert await api.post(BASE + 'print-data', query) == printed

async def test_revision_change_during_reference_lookup_does_not_save_snapshot(api, invoice_references, mockserver, pgsql):
    body = invoice([])
    await api.post(BASE + 'save', body)

    @mockserver.json_handler('/api/v1/counterparties/get')
    def changed_during_lookup(request):
        with pgsql['db_1'].cursor() as cursor:
            cursor.execute('UPDATE invoices SET revision = revision + 1 WHERE id = %s::UUID', (body['id'],))
        return {'counterparty': {
            'id': request.json['id'], 'name': 'Компания', 'revision': 1, 'own_company': True,
            'archived': False, 'country_code': None, 'tax_id': None,
            'registration_id': None, 'legal_name': None, 'legal_address': None,
            'roles': [], 'contacts': [],
        }}

    await api.post(BASE + 'print-data', dict(id=body['id'], revision=1), expected=409)
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT COUNT(*) FROM invoice_print_snapshots WHERE invoice_id = %s::UUID', (body['id'],))
        assert cursor.fetchone()[0] == 0
