"""Начисления: расчёт, снимки, границы организации и сводка без двойного учёта."""
import asyncio
from uuid import uuid4
import pytest
from test_financial_labels import label

BASE = '/api/v1/service-charges/'
SHIPMENT = 'aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa'
TRANSPORT = 'bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb'
PARTY = 'cccccccc-cccc-4ccc-8ccc-cccccccccccc'
CONTRACT = 'dddddddd-dddd-4ddd-8ddd-dddddddddddd'


@pytest.fixture
def references(mockserver):
    state = {'shipment': SHIPMENT, 'currency': 'RUB', 'party': PARTY, 'archived': False, 'status': 200}

    @mockserver.json_handler('/api/v1/shipments/get')
    def shipment(request):
        if state['status'] != 200:
            return mockserver.make_response(status=state['status'], json={'error': 'unavailable', 'message': 'Недоступно'})
        return mockserver.make_response(json={'shipment': {
            'id': request.json['shipment_id'], 'revision': 1, 'number': 'S-1',
            'customer_id': PARTY, 'reference': '', 'shipment_date': '2026-10-30',
            'status': 'Новая', 'route_segments': [],
        }}, status=state['status'])

    @mockserver.json_handler('/api/v1/transportations/get')
    def transportation(request):
        if state.get('transport_status', 200) != 200:
            return mockserver.make_response(status=state['transport_status'], json={'error': 'unavailable', 'message': 'Недоступно'})
        return {'transportation': {
            'id': request.json['transportation_id'], 'shipment_id': state['shipment'], 'revision': 1,
            'container_number': 'ABCD1234567', 'container_type_code': '40HC', 'container_ownership': 'COC',
            'type': 'container', 'hazardous': False, 'route_segments': [],
        }}

    @mockserver.json_handler('/api/v1/counterparties/get')
    def counterparty(request):
        if state.get('party_status', 200) != 200:
            return mockserver.make_response(status=state['party_status'], json={'error': 'unavailable', 'message': 'Недоступно'})
        return {'counterparty': {
            'id': request.json['id'], 'name': 'Перевозчик', 'revision': 1, 'own_company': False,
            'archived': state['archived'], 'country_code': None, 'tax_id': None,
            'registration_id': None, 'legal_name': None, 'legal_address': None,
            'roles': [], 'contacts': [],
        }}

    @mockserver.json_handler('/api/v1/contracts/get')
    def contract(request):
        if state.get('contract_status', 200) != 200:
            return mockserver.make_response(status=state['contract_status'], json={'error': 'unavailable', 'message': 'Недоступно'})
        return {'contract': {
            'id': request.json['id'], 'own_company_id': SHIPMENT, 'counterparty_id': state['party'],
            'number': 'Д-1', 'contract_date': '2026-10-30', 'valid_until': None,
            'currency': state['currency'], 'own_bank_account_id': None,
            'counterparty_bank_account_id': None, 'payment_delay_days': 30,
            'note': None, 'archived': state['archived'], 'revision': 1,
        }}
    return state


@pytest.fixture
def scope():
    return {'shipment_id': SHIPMENT, 'transportation_id': None}


@pytest.fixture
def charge(api, references, scope):
    async def create(**changes):
        saved = (await api.post('/api/v1/financial-labels/save', label(currency='RUB')))['label']
        return {
            'id': str(uuid4()), **scope, 'label_id': saved['id'], 'direction': 'income',
            'unit': 'transportation', 'counterparty_id': PARTY, 'contract_id': CONTRACT,
            'quantity_milli': 1000, 'price_minor': 10000, 'price_mode': 'net', 'currency': 'RUB',
            'vat_mode': 'rate', 'vat_rate_basis_points': 2000, 'note': None,
            'archived': False, 'expected_revision': 0, **changes,
        }
    return create


async def listing(api, scope, **changes):
    return await api.post(BASE + 'list', {'scope': scope, 'after': None, 'limit': 50, 'include_archived': False, **changes})


@pytest.mark.parametrize('mode,price,quantity,rate,amounts', [
    ('net', 10000, 1000, 2000, (10000, 2000, 12000)),
    ('gross', 12000, 1000, 2000, (10000, 2000, 12000)),
    ('net', 1, 500, 2000, (1, 0, 1)),
    ('gross', 1, 1000, 10000, (0, 1, 1)),
    ('net', 111, 1500, 2250, (167, 38, 205)),
    ('gross', 111, 1500, 2250, (136, 31, 167)),
    ('net', 10000, 1000, 0, (10000, 0, 10000)),
    ('gross', 100000000, 10000000, 10000, (500000000000, 500000000000, 1000000000000)),
])
async def test_exact_amounts(api, charge, mode, price, quantity, rate, amounts):
    value = await charge(price_mode=mode, price_minor=price, quantity_milli=quantity, vat_rate_basis_points=rate)
    saved = (await api.post(BASE + 'save', value))['charge']
    assert tuple(saved[key] for key in ['net_minor', 'vat_minor', 'total_minor']) == amounts
    assert saved['price_mode'] == mode


async def test_scope_totals_without_double_counting(api, charge, scope):
    for changes in [{}, {'transportation_id': TRANSPORT}, {'transportation_id': TRANSPORT, 'direction': 'expense', 'currency': 'USD', 'contract_id': None}]:
        await api.post(BASE + 'save', await charge(**changes))
    result = await listing(api, scope)
    assert len(result['items']) == 1
    assert result['summary']['own'] == [{'currency': 'RUB', 'income_minor': '12000', 'expense_minor': '0'}]
    assert result['summary']['combined'] == [
        {'currency': 'RUB', 'income_minor': '24000', 'expense_minor': '0'},
        {'currency': 'USD', 'income_minor': '0', 'expense_minor': '12000'},
    ]
    transport = await listing(api, {**scope, 'transportation_id': TRANSPORT})
    assert len(transport['items']) == 2
    assert transport['summary']['own'] == transport['summary']['combined']
    assert transport['summary']['transportations'] == []


async def test_snapshot_archive_and_history(api, charge, scope, pgsql):
    value = await charge()
    saved = (await api.post(BASE + 'save', value))['charge']
    # Изменение справочника не затрагивает сохранённые суммы и названия строки.
    await api.post('/api/v1/financial-labels/save', label(id=value['label_id'], name='Новое имя', expected_revision=1, vat_mode='exempt', vat_rate_basis_points=None))
    assert (await listing(api, scope))['items'][0] == saved
    await api.post(BASE + 'save', {**value, 'archived': True, 'expected_revision': 1})
    result = await listing(api, scope)
    assert result['items'] == [] and result['summary']['combined'] == []
    assert len((await listing(api, scope, include_archived=True))['items']) == 1
    history = pgsql['db_1'].cursor()
    history.execute('SELECT revision FROM service_charge_history WHERE service_charge_id=%s ORDER BY revision', (value['id'],))
    assert history.fetchall() == [(1,), (2,)]


async def test_scope_is_immutable_and_revision_protected(api, charge, scope):
    value = await charge()
    await api.post(BASE + 'save', value)
    results = await asyncio.gather(*[api.post(BASE + 'save', {**value, 'expected_revision': 1, 'price_minor': price}, expected=(200, 409)) for price in [20000, 30000]])
    assert sum('charge' in result for result in results) == 1
    await api.post(BASE + 'save', {**value, 'expected_revision': 2, 'transportation_id': TRANSPORT}, expected=409)
    assert len((await listing(api, scope))['items']) == 1


async def test_reference_validation(api, charge, references, scope):
    value = await charge(transportation_id=TRANSPORT)
    references['shipment'] = str(uuid4())
    await api.post(BASE + 'save', value, expected=400)
    await api.post(BASE + 'list', {'scope': {**scope, 'transportation_id': TRANSPORT}, 'after': None, 'limit': 50, 'include_archived': False}, expected=400)
    references['shipment'] = SHIPMENT
    references['currency'] = 'USD'
    await api.post(BASE + 'save', value, expected=400)
    references['currency'] = 'RUB'; references['party'] = str(uuid4())
    await api.post(BASE + 'save', value, expected=400)
    references['party'] = PARTY; references['archived'] = True
    await api.post(BASE + 'save', value, expected=409)


@pytest.mark.parametrize('changes', [
    {'quantity_milli': 0}, {'quantity_milli': 10000001}, {'quantity_milli': 1.1},
    {'price_minor': -1}, {'price_minor': 100000001}, {'price_mode': 'unknown'},
    {'vat_mode': 'exempt'}, {'vat_rate_basis_points': None}, {'vat_rate_basis_points': 10001},
    {'expected_revision': 9223372036854775807}, {'direction': 'both'},
])
async def test_invalid_values(api, charge, changes):
    await api.post(BASE + 'save', await charge(**changes), expected=400)


async def test_organization_permissions_and_pagination(api, charge, scope):
    ids = []
    for _ in range(3):
        value = await charge(); ids.append(value['id']); await api.post(BASE + 'save', value)
    first = await listing(api, scope, limit=1)
    second = await listing(api, scope, limit=1, after=first['next_after'])
    assert first['items'][0]['id'] != second['items'][0]['id']
    assert len(first['summary']['own']) == 1 and first['summary']['own'][0]['income_minor'] == '36000'
    result = await api.post(BASE+'list', {'scope':scope,'after':None,'limit':50,'include_archived':False}, user='other')
    assert result['items'] == [] and result['summary']['combined'] == []
    await api.post(BASE+'save', await charge(), roles=['finance.read_service_charges'], expected=403)
    await api.post(BASE+'list', {'scope':scope,'after':None,'limit':50,'include_archived':False}, roles=['finance.read_financial_labels'], expected=403)


async def test_dependency_errors_do_not_create_entry(api, charge, references, scope):
    value = await charge()
    references['status'] = 404
    await api.post(BASE+'save', value, expected=404)
    references['status'] = 503
    await api.post(BASE+'save', value, expected=502)
    references['status'] = 200
    assert (await listing(api, scope))['items'] == []


@pytest.mark.parametrize('target', ['status', 'transport_status', 'party_status', 'contract_status'])
@pytest.mark.parametrize('status,expected', [(403,403),(404,404),(503,502)])
async def test_shared_http_status_errors(api, charge, references, target, status, expected):
    value = await charge(**({'transportation_id': TRANSPORT} if target == 'transport_status' else {}))
    references[target] = status
    result = await api.post(BASE+'save', value, expected=expected)
    assert result['error'] == {403:'forbidden_reference',404:'reference_not_found',502:'reference_unavailable'}[expected]
