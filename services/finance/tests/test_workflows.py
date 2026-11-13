"""Пустой счёт, самостоятельные услуги и атомарное создание услуги из счёта."""
import asyncio
import pytest
from conftest import ORGANIZATION
from test_service_charges import references, scope, charge
from test_invoices import invoice, invoice_references
from test_exchange_rates import finance_admin

async def test_empty_invoice_can_be_saved_and_filled(api, charge, invoice_references):
    document = invoice([])
    saved = await api.post('/api/v1/invoices/save', document)
    assert saved['charges'] == []
    assert saved['invoice']['total_minor'] == 0
    body = await charge(shipment_id=None, transportation_id=None)
    saved_charge = await api.post('/api/v1/service-charges/save', {
        **body, 'target_invoice_id': document['id'], 'target_invoice_revision': 1})
    result = await api.post('/api/v1/invoices/get', {'id': document['id']})
    assert result['charges'] == [saved_charge['charge']]
    assert result['invoice']['total_minor'] == saved_charge['charge']['invoice_total_minor']
    assert result['charges'][0]['shipment_id'] is None

async def test_invoice_conflict_rolls_back_new_service(api, charge, invoice_references):
    document = invoice([])
    await api.post('/api/v1/invoices/save', document)
    body = await charge(shipment_id=None)
    await api.post('/api/v1/service-charges/save', {
        **body, 'target_invoice_id': document['id'], 'target_invoice_revision': 0}, expected=409)
    await api.post('/api/v1/service-charges/get', {'id': body['id']}, expected=404)
    assert (await api.post('/api/v1/invoices/get', {'id':document['id']}))['charges'] == []

async def test_organization_services_include_standalone_and_bound(api, charge, references, scope):
    standalone = await charge(shipment_id=None)
    bound = await charge()
    for body in [standalone, bound]: await api.post('/api/v1/service-charges/save', body)
    result = await api.post('/api/v1/service-charges/list', {
        'scope': {'shipment_id':None,'transportation_id':None},
        'after':None,'limit':50,'include_archived':False})
    assert {item['id'] for item in result['items']} == {standalone['id'],bound['id']}
    assert result['summary']['combined'] == []  # Общий реестр не пересчитывает все услуги организации.
    shipment = await api.post('/api/v1/service-charges/list', {
        'scope':scope,'after':None,'limit':50,'include_archived':False})
    assert [item['id'] for item in shipment['items']] == [bound['id']]
    assert (await api.post('/api/v1/service-charges/list', {
        'scope': {'shipment_id':None,'transportation_id':None},
        'after':None,'limit':50,'include_archived':False},user='other'))['items'] == []

async def test_transportation_still_requires_shipment(api,charge,references):
    body = await charge(shipment_id=None,transportation_id='33333333-3333-4333-8333-333333333333')
    await api.post('/api/v1/service-charges/save',body,expected=400)

async def test_invoice_number_masks_and_independent_atomic_sequences(api,invoice_references,finance_admin):
    path='/api/admin/finance/settings/'
    rules={'organization_id':ORGANIZATION,'reporting_currency':'RUB',
        'outgoing_number_template':'OUT-{sequence}/{yy}{mm}{dd}',
        'incoming_number_template':'IN-{sequence}/{yyyy}',
        'reset_numbers_yearly':True,'expected_revision':0}
    await api.post(path+'update',rules)
    async def create(direction='outgoing',date='2026-10-30'):
        return (await api.post('/api/v1/invoices/save', {**invoice([]),
            'direction':direction,'number':None,'invoice_date':date}))['invoice']
    first, second = await asyncio.gather(create(),create())
    assert {first['number'],second['number']} == {'OUT-1/261030','OUT-2/261030'}
    assert (await create('incoming'))['number'] == 'IN-1/2026'
    assert (await create(date='2027-01-01'))['number'] == 'OUT-1/270101'
    await api.post(path+'update',{**rules,'outgoing_number_template':'bad','expected_revision':1},expected=400)
    await api.post(path+'update',rules,expected=409)


async def test_standalone_service_can_be_edited_after_creating_its_invoice(api, charge, invoice_references):
    body = await charge(shipment_id=None, transportation_id=None, vat_mode='exempt', vat_rate_basis_points=None)
    saved = (await api.post('/api/v1/service-charges/save', body))['charge']
    document = invoice([saved['id']])
    await api.post('/api/v1/invoices/save', document)
    updated = (await api.post('/api/v1/service-charges/save', {
        **body, 'expected_revision': saved['revision'], 'price_minor': 25000,
    }))['charge']
    assert updated['revision'] == saved['revision'] + 1
    result = await api.post('/api/v1/invoices/get', {'id': document['id']})
    assert result['charges'] == [updated]
    assert result['invoice']['total_minor'] == updated['invoice_total_minor']
    # Защита от настоящего конфликта ревизий сохраняется.
    await api.post('/api/v1/service-charges/save', {
        **body, 'expected_revision': saved['revision'],
    }, expected=409)

@pytest.mark.parametrize('kind', ['invoices', 'payments'])
@pytest.mark.parametrize('status, expected, code', [
    (403, 403, 'forbidden_reference'),
    (404, 404, 'reference_not_found'),
    (503, 502, 'reference_unavailable'),
])
async def test_document_reference_failure_does_not_create_document(
    api, mockserver, pgsql, invoice_references, kind, status, expected, code,
):
    from test_invoices import invoice
    from test_payments import payment

    body = invoice([]) if kind == 'invoices' else payment()

    @mockserver.json_handler('/api/v1/counterparties/get')
    def unavailable(request):
        return mockserver.make_response(
            status=status, json={'error': 'unavailable', 'message': 'Недоступно'},
        )

    result = await api.post(f'/api/v1/{kind}/save', body, expected=expected)
    assert result['error'] == code
    cursor = pgsql['db_1'].cursor()
    # Имя таблицы выбирается только из параметров теста выше.
    cursor.execute(f'SELECT COUNT(*) FROM {kind} WHERE id = %s::UUID', (body['id'],))
    assert cursor.fetchone()[0] == 0

async def test_service_charge_without_invoice_permission_cannot_change_linked_invoice(
    api, charge, invoice_references,
):
    from test_invoices import saved_entry

    body, saved = await saved_entry(api, charge)
    document = invoice([saved['id']])
    before = await api.post('/api/v1/invoices/save', document)
    roles = [
        'finance.write_service_charges', 'finance.read_service_charges',
        'finance.read_invoices', 'shipments.read', 'transportations.read',
        'contracts.read', 'counterparties.read',
    ]
    result = await api.post('/api/v1/service-charges/save', {
        **body, 'expected_revision': 1, 'price_minor': 50000,
    }, roles=roles, expected=403)
    assert result['error'] == 'forbidden'
    assert await api.post('/api/v1/service-charges/get', {'id': body['id']}) == {'charge': saved}
    assert await api.post('/api/v1/invoices/get', {'id': document['id']}) == before
