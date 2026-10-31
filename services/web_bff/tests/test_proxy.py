"""Вход проходит через настоящую проверку JWT; бизнес-сервисы мокируются на HTTP-границе."""

from uuid import uuid4
import pytest

ID = str(uuid4())
CUSTOMER = str(uuid4())
SHIPMENT = {
    'id': ID,
    'revision': 1,
    'number': 'S-1',
    'customer_id': '33333333-3333-4333-8333-333333333333',
    'reference': 'Тест',
    'shipment_date': '2026-10-22',
    'status': 'Новая',
    'route_segments': [],
}
ITEM = {
    'id': ID,
    'revision': 1,
    'shipment_id': ID,
    'container_type_code': '40HC',
    'container_ownership': 'COC',
    'planned_quantity': 5,
    'created_quantity': 0,
    'excess_quantity': 0,
    'pending_quantity': 0,
    'available_quantity': 5,
}
PLAN = {key: value for key,value in ITEM.items() if key not in {'available_quantity','created_quantity','pending_quantity','excess_quantity'}}

OPERATION = {'id': ID, 'item_id': ID, 'quantity': 3, 'status': 'pending', 'transportation_ids': []}
TRANSPORTATION = {
    'type': 'container',
    'hazardous': False,
    'id': ID,
    'container_number': 'CSQU3054383',
    'revision': 1,
    'shipment_id': ID,
    'item_id': ID,
    'operation_id': ID,
    'container_type_code': '40HC',
    'container_ownership': 'COC',
    'route_segments': [],
}

COUNTERPARTY = {
    'own_company': False,
    'id': ID,
    'name': 'Клиент',
    'revision': 1,
    'archived': False,
    'country_code': None,
    'tax_id': None,
    'registration_id': None,
    'legal_name': None,
    'legal_address': None,
    'roles': [],
    'contacts': [],
}


BANK_ACCOUNT = {
    'id': ID, 'counterparty_id': ID, 'name': 'Основной', 'number': '40702810000000000001',
    'currency': 'RUB', 'bank_name': 'Банк', 'bic': '044525225', 'swift': None,
    'iban': None, 'correspondent_account': None, 'is_primary': True, 'archived': False, 'revision': 1,
}


CONTRACT = {
    'id': ID, 'own_company_id': ID, 'counterparty_id': CUSTOMER, 'number':'Д-1',
    'contract_date':'2026-10-30','valid_until':None,'currency':'RUB',
    'own_bank_account_id':None,'counterparty_bank_account_id':None,
    'payment_delay_days':30,'note':None,'archived':False,'revision':1,
}


FINANCIAL_LABEL = {
    'id':ID,'name':'Фрахт','direction':'both','unit':'container','currency':'USD',
    'vat_mode':'rate','vat_rate_basis_points':0,'note':None,'archived':False,'revision':1,
}


SERVICE_CHARGE = {
    'id': ID, 'shipment_id': ID, 'transportation_id': None, 'label_id': ID,
    'label_name': 'Фрахт', 'direction': 'income', 'unit': 'container',
    'counterparty_id': CUSTOMER, 'counterparty_name': 'Клиент',
    'contract_id': None, 'contract_number': None, 'quantity_milli': 1500,
    'price_minor': 12000, 'price_mode': 'gross', 'currency': 'RUB',
    'vat_mode': 'rate', 'vat_rate_basis_points': 2000,
    'net_minor': 15000, 'vat_minor': 3000, 'total_minor': 18000,
    'note': None, 'archived': False, 'revision': 1,
}
SERVICE_CHARGE.update(service_date='2026-10-30',invoice_currency='RUB',
    invoice_rate={'date':'2026-10-30','rate_millionths':1000000,'source':'identity'},
    invoice_net_minor=15000,invoice_vat_minor=3000,invoice_total_minor=18000)
SERVICE_CHARGE_SAVE = {key:value for key,value in SERVICE_CHARGE.items()
              if key not in ('label_name','counterparty_name','contract_number','net_minor','vat_minor','total_minor','revision','invoice_rate','invoice_net_minor','invoice_vat_minor','invoice_total_minor')}
SERVICE_CHARGE_SAVE['expected_revision'] = 0
SERVICE_CHARGE_SAVE['manual_invoice_rate_millionths'] = None
SERVICE_CHARGE_SUMMARY = {'own':[{'currency':'RUB','income_minor':'18000','expense_minor':'0'}],
                 'transportations':[], 'combined':[{'currency':'RUB','income_minor':'18000','expense_minor':'0'}]}


INVOICE = {
    'id':ID,'direction':'outgoing','number':'ИС-1','invoice_date':'2026-10-30',
    'due_date':None,'own_company_id':ID,'own_company_name':'Своя компания',
    'counterparty_id':CUSTOMER,'counterparty_name':'Клиент',
    'own_bank_account_id':None,'counterparty_bank_account_id':None,
    'contract_id':None,'contract_number':None,'currency':'RUB','note':None,
    'archived':False,'revision':1,'net_minor':15000,'vat_minor':3000,'total_minor':18000,
}
INVOICE_SAVE={key:value for key,value in INVOICE.items() if key not in
              ('own_company_name','counterparty_name','contract_number','revision','net_minor','vat_minor','total_minor')}
INVOICE_SAVE.update(service_charge_ids=[ID],expected_revision=0)

PAYMENT = {key: value for key,value in INVOICE.items() if key in ('id','number','own_company_id','own_company_name','counterparty_id','counterparty_name','own_bank_account_id','counterparty_bank_account_id','currency','note','archived','revision')}
PAYMENT.update(kind='receipt',payment_date='2026-10-30',amount_minor=20000,purpose='Оплата услуг')
PAYMENT_SAVE={key:value for key,value in PAYMENT.items() if key not in ('revision','own_company_name','counterparty_name')}
PAYMENT_SAVE.update(expected_revision=0,allocations=[{'invoice_id':ID,'amount_minor':18000}])
PAYMENT_DETAILS={'payment':PAYMENT,'allocations':PAYMENT_SAVE['allocations'],'allocated_minor':18000,'unallocated_minor':2000}

@pytest.mark.parametrize(
    'path,body,result,status',
    [
        ('payments/payment-candidates',{'invoice_id':ID,'limit':40},{'items':[{'payment':PAYMENT,'unallocated_minor':2000}],'next_after':None},200),
        ('payments/save',PAYMENT_SAVE,PAYMENT_DETAILS,200),
        ('payments/get',{'id':ID},PAYMENT_DETAILS,200),
        ('payments/list',{'kind':'receipt','sort':'payment_date','descending':True,'filters':[],'limit':50},{'items':[PAYMENT]},200),
        ('payments/invoice-balance',{'id':ID},{'invoice':INVOICE,'paid_minor':10000,'outstanding_minor':8000,'payments':[]},200),
        ('payments/invoice-candidates',{'kind':'receipt','own_company_id':ID,'counterparty_id':CUSTOMER,'currency':'RUB','limit':40},{'items':[{'invoice':INVOICE,'paid_minor':10000,'outstanding_minor':8000,'payments':[]}],'next_after':None},200),
        ('payments/filter-values',{'registry':{'kind':'receipt','sort':'id','descending':False,'limit':50}},{'fields':[],'values':[]},200),
        ('invoices/save',INVOICE_SAVE,{'invoice':INVOICE,'charges':[SERVICE_CHARGE]},200),
        ('invoices/detach-service-charge',{'invoice_id':ID,'service_charge_id':ID},{'invoice':INVOICE,'charges':[SERVICE_CHARGE]},200),
        ('invoices/add-service-charge',{'invoice_id':ID,'service_charge_id':ID,'expected_revision':1},{'invoice':INVOICE,'charges':[SERVICE_CHARGE]},200),
        ('service-charges/get',{'id':ID},{'charge':SERVICE_CHARGE},200),
        ('service-charges/filter-values',{'registry':{'sort':'service_date','descending':True,'limit':50}},{'fields':[],'values':[]},200),
        ('service-charges/registry',{'sort':'service_date','descending':True,'limit':50,'filters':[]},{'items':[SERVICE_CHARGE],'invoices':[]},200),
        ('invoices/filter-values',{'registry':{'direction':'incoming','sort':'id','descending':False,'limit':50}},{'fields':[],'values':[]},200),
        ('invoices/get',{'id':ID},{'invoice':INVOICE,'charges':[SERVICE_CHARGE]},200),
        ('service-charges/history',{'id':ID,'limit':30},{'items':[{'actor':'issuer#author','actor_name':'Автор','title':'Изменение','has_previous':True,'cursor':{'source':'service_charge','entity_id':ID,'revision':2,'occurred_at':'2026-10-31T12:00:00+00:00'},'changes':[{'field':'note','label':'Примечание','before':{'format':'text','value':'Было'},'after':{'format':'text','value':'Стало'}}]}]},200),
        ('invoices/history',{'id':ID,'limit':30},{'items':[{'actor':'issuer#author','actor_name':'Автор','title':'Изменение','has_previous':True,'cursor':{'source':'invoice','entity_id':ID,'revision':2,'occurred_at':'2026-10-31T12:00:00+00:00'},'changes':[{'field':'note','label':'Примечание','before':{'format':'text','value':'Было'},'after':{'format':'text','value':'Стало'}}]}]},200),
        ('payments/history',{'id':ID,'limit':30},{'items':[{'actor':'issuer#author','actor_name':'Автор','title':'Изменение','has_previous':True,'cursor':{'source':'payment','entity_id':ID,'revision':2,'occurred_at':'2026-10-31T12:00:00+00:00'},'changes':[{'field':'note','label':'Примечание','before':{'format':'text','value':'Было'},'after':{'format':'text','value':'Стало'}}]}]},200),
        ('invoices/print-data',{'id':ID,'revision':1},{'invoice':INVOICE,'charges':[SERVICE_CHARGE],'supplier':{'name':'Компания','tax_id':None,'kpp':None,'registration_id':None,'legal_address':None},'customer':{'name':'Компания','tax_id':None,'kpp':None,'registration_id':None,'legal_address':None}},200),
        ('invoices/list',{'direction':'outgoing','sort':'invoice_date','limit':50,'filters':[]},{'items':[{'invoice':INVOICE,'paid_minor':0,'outstanding_minor':INVOICE['total_minor'],'payment_status':'unpaid','overdue_days':0}]},200),
        ('invoices/candidates',{'direction':'outgoing','counterparty_id':CUSTOMER,'currency':'RUB','contract_id':None,'invoice_id':None,'after':None,'limit':50},{'items':[SERVICE_CHARGE],'next_after':None},200),
        ('service-charges/save', SERVICE_CHARGE_SAVE, {'charge':SERVICE_CHARGE},200),
        ('service-charges/list', {'scope':{'shipment_id':ID,'transportation_id':None},'after':None,'limit':50,'include_archived':False}, {'items':[SERVICE_CHARGE],'next_after':None,'summary':SERVICE_CHARGE_SUMMARY,'invoices':[],'reporting':{'currency':'RUB','summary':SERVICE_CHARGE_SUMMARY,'missing_rates':[]}},200),
        ('exchange-rates/get',{'from_currency':'USD','to_currency':'RUB','date':'2026-10-30'},{'date':'2026-10-30','rate_millionths':90000000,'source':'cbr'},200),
        ('finance/settings/get',{}, {'outgoing_number_template':'ИС-{sequence}/{yyyy}','incoming_number_template':'ВС-{sequence}/{yyyy}','reset_numbers_yearly':False,'reporting_currency':'USD','revision':1},200),
        ('financial-labels/get',{'id':ID},{'label':FINANCIAL_LABEL},200),
        ('financial-labels/list',{}, {'items':[FINANCIAL_LABEL]},200),
        ('financial-labels/save',{**{key:value for key,value in FINANCIAL_LABEL.items() if key!='revision'},'expected_revision':0},{'label':FINANCIAL_LABEL},200),
        ('contracts/get', {'id':ID}, {'contract':CONTRACT},200),
        ('contracts/list', {}, {'items':[CONTRACT]},200),
        ('contracts/save', {**{key:value for key,value in CONTRACT.items() if key != 'revision'},'expected_revision':0}, {'contract':CONTRACT},200),
        ('counterparties/bank-accounts/list', {'counterparty_id': ID}, {'items':[BANK_ACCOUNT]}, 200),
        ('counterparties/bank-accounts/save', {**{key:value for key,value in BANK_ACCOUNT.items() if key != 'revision'}, 'expected_revision':0}, {'account':BANK_ACCOUNT}, 200),
        (
            'counterparties/create',
            {'id': ID, 'name': 'Клиент'},
            {'counterparty': COUNTERPARTY},
            201,
        ),
        ('counterparties/list', {}, {'items': [COUNTERPARTY]}, 200),
        ('counterparties/get', {'id': ID}, {'counterparty': COUNTERPARTY}, 200),
        (
            'counterparties/update',
            {
                'id': ID,
                'expected_revision': 1,
                'name': 'Клиент',
                'roles': ['agent'],
                'contacts': [{'name': 'Анна', 'position': None, 'email': None, 'phone': None}],
                'legal_name': None,
            },
            {'counterparty': COUNTERPARTY},
            200,
        ),
        (
            'counterparties/set-archived',
            {'id': ID, 'expected_revision': 1, 'archived': True},
            {'counterparty': {**COUNTERPARTY, 'revision': 2, 'archived': True}},
            200,
        ),
        (
            'shipments/create',
            {'customer_id': '33333333-3333-4333-8333-333333333333', 'idempotency_key': ID},
            {'shipment': SHIPMENT},
            201,
        ),
        ('shipments/get', {'shipment_id': ID}, {'shipment': SHIPMENT}, 200),
        ('shipments/list', {'limit': 50}, {'items': [SHIPMENT]}, 200),
        (
            'shipments/update',
            {'shipment_id': ID, 'expected_revision': 1, 'reference': 'Тест'},
            {'shipment': SHIPMENT},
            200,
        ),
        (
            'shipment-items/add',
            {
                'shipment_id': ID,
                'expected_revision': 1,
                'container_type_code': '40HC',
                'container_ownership': 'COC',
                'planned_quantity': 5,
                'idempotency_key': ID,
            },
            {'item': PLAN},
            201,
        ),
        (
            'shipments/settings',
            {},
            {
                'customer_logist': None,
                'number_template': 'S-{sequence}',
                'reset_number_yearly': False,
                'statuses': ['Новая'],
                'route_types': ['sea'],
                'planned_containers': True,
                'container_types': ['40HC'],
            },
            200,
        ),
        (
            'shipments/dictionaries',
            {},
            {
                'incoterms': [{'code': 'FOB', 'name': 'FOB'}],
                'carriage_terms': [],
                'loading': [],
                'route_types': [{'code': 'sea', 'name': 'Море'}],
            },
            200,
        ),
        (
            'organizations/employees/list',
            {'query': 'Иван'},
            {'items': [{'subject': 'employee-1', 'display_name': 'Иванов Иван'}]},
            200,
        ),
        (
            'shipments/cargo-suggestions',
            {'customer_id': '33333333-3333-4333-8333-333333333333', 'query': 'груз'},
            {'items': ['Груз']},
            200,
        ),
        (
            'shipments/cargo-suggestions/remove',
            {'customer_id': '33333333-3333-4333-8333-333333333333', 'value': 'Груз'},
            {'deleted': True},
            200,
        ),
        (
            'shipment-items/list',
            {'shipment_id': ID},
            {'items': [ITEM], 'unplanned_items': []},
            200,
        ),
        (
            'shipment-items/update',
            {'item_id': ID, 'expected_revision': 1, 'planned_quantity': 5},
            {'item': PLAN},
            200,
        ),
        (
            'transportation-operations/create',
            {'item_id': ID, 'expected_revision': 1, 'quantity': 3, 'idempotency_key': ID},
            {'operation': OPERATION},
            202,
        ),
        ('transportation-operations/get', {'operation_id': ID}, {'operation': OPERATION}, 200),
        (
            'transportations/get',
            {'transportation_id': ID},
            {'transportation': TRANSPORTATION},
            200,
        ),
        (
            'transportations/list',
            {'filters': [{'field': 'shipment_id', 'values': [ID]}]},
            {'items': [TRANSPORTATION]},
            200,
        ),
    ],
)
async def test_typed_proxy(browser, mockserver, path, body, result, status):
    await browser.sign_in()

    endpoint = '/api/' + path if path.startswith('organizations/') else '/api/v1/' + path

    @mockserver.json_handler(endpoint)
    def upstream(request):
        assert request.headers['Authorization'].startswith('Bearer ')
        assert 'broker_web' not in request.headers.get('Cookie', '')
        for key, value in body.items():
            assert request.json[key] == value
        return mockserver.make_response(status=status, json=result)

    response = await browser.post(endpoint, body)
    assert response.status == status, response.text
    assert response.json() == result


async def test_csrf_and_upstream_errors(browser, mockserver):
    await browser.sign_in()

    @mockserver.json_handler('/api/v1/shipments/list')
    def upstream(request):
        return mockserver.make_response(
            status=403, json={'error': 'forbidden', 'message': 'Нет доступа'}
        )

    assert (
        await browser.post('/api/v1/shipments/list', {}, headers={'Origin': 'http://untrusted'})
    ).status == 403
    assert upstream.times_called == 0
    response = await browser.post('/api/v1/shipments/list', {})
    assert response.status == 403
    assert response.json()['message'] == 'Нет доступа'


async def test_admin_cookie_does_not_authenticate_web(service_client, browser):
    await browser.sign_in()
    response = await service_client.get(
        '/api/session',
        headers={'Cookie': browser.cookie.replace('broker_web=', 'broker_admin=')},
        cookies={'broker_web': ''},
    )
    assert response.status == 401


@pytest.mark.parametrize(
    'path,limit', [('shipments', 50), ('transportations', 50), ('counterparties', 100)]
)
async def test_converted_request_defaults(browser, mockserver, path, limit):
    await browser.sign_in()

    @mockserver.json_handler('/api/v1/' + path + '/list')
    def upstream(request):
        expected = {'limit': limit, 'sort': 'id', 'descending': False}
        assert request.json == expected
        return {'items': []}

    response = await browser.post('/api/v1/' + path + '/list', {})
    assert response.status == 200, response.text
    assert upstream.times_called == 1


@pytest.mark.parametrize('mode', ['road', 'rail', 'sea'])
@pytest.mark.parametrize('ownership', ['SOC', 'COC'])
async def test_converted_transportation_with_route(browser, mockserver, mode, ownership):
    await browser.sign_in()
    transportation = {
        **TRANSPORTATION,
        'container_ownership': ownership,
        'route_segments': [
            {
                'id': str(uuid4()),
                'mode': mode,
                'origin': {'name': 'Шанхай', 'country_code': 'CN'},
                'destination': {'name': 'Владивосток', 'country_code': 'RU'},
                'contractor_id': str(uuid4()),
                'planned_departure_at': '2026-10-21T10:00:00+00:00',
                'planned_arrival_at': '2026-10-23T10:00:00+00:00',
                'actual_departure_at': '2026-10-21T11:00:00+00:00',
            }
        ],
    }

    @mockserver.json_handler('/api/v1/transportations/get')
    def upstream(request):
        assert request.json == {'transportation_id': ID}
        return {'transportation': transportation}

    response = await browser.post('/api/v1/transportations/get', {'transportation_id': ID})
    assert response.status == 200, response.text
    assert response.json() == {'transportation': transportation}


@pytest.mark.parametrize(
    'path,body,result',
    [
        (
            'transportation-routes/set',
            {
                'transportation_id': ID,
                'expected_revision': 1,
                'route_segments': [
                    {
                        'id': ID,
                        'mode': 'sea',
                        'origin': {'name': 'Шанхай', 'country_code': 'CN'},
                        'destination': {'name': 'Владивосток', 'country_code': 'RU'},
                        'contractor_id': CUSTOMER,
                        'planned_departure_at': '2026-10-21T10:00:00+00:00',
                        'planned_arrival_at': '2026-10-23T10:00:00+00:00',
                    }
                ],
            },
            {'transportation': TRANSPORTATION},
        ),
        (
            'transportation-actuals/record',
            {
                'transportation_id': ID,
                'expected_revision': 2,
                'route_segment_id': ID,
                'event_kind': 'arrival',
                'occurred_at': '2026-10-23T10:00:00+00:00',
            },
            {'transportation': TRANSPORTATION},
        ),
        (
            'transportation-actuals/correct',
            {
                'transportation_id': ID,
                'expected_revision': 3,
                'route_segment_id': ID,
                'event_kind': 'departure',
                'occurred_at': '2026-10-21T11:00:00+00:00',
                'reason': 'Уточнение',
            },
            {'transportation': TRANSPORTATION},
        ),
        (
            'transportations/history',
            {'transportation_id': ID, 'before': 10, 'limit': 5},
            {
                'items': [
                    {
                        'id': 9,
                        'action': 'correct_actual',
                        'actor': 'issuer#subject',
                        'actor_name': 'Логист',
                        'occurred_at': '2026-10-21T11:00:00+00:00',
                        'reason': 'Уточнение',
                        'event_kind': 'departure',
                        'route_segment_id': ID,
                        'revision': 4,
                        'route_segments': TRANSPORTATION['route_segments'],
                    }
                ]
            },
        ),
    ],
)
async def test_transportation_commands_and_history(browser, mockserver, path, body, result):
    await browser.sign_in()

    endpoint = '/api/' + path if path.startswith('organizations/') else '/api/v1/' + path

    @mockserver.json_handler(endpoint)
    def upstream(request):
        assert request.json == body
        assert request.headers['Authorization'].startswith('Bearer ')
        return result

    response = await browser.post(endpoint, body)
    assert response.status == 200, response.text
    assert response.json() == result


@pytest.mark.parametrize('patch', [{}, {'legal_name': None}, {'legal_name': 'Новое имя'}])
async def test_counterparty_patch_presence(browser, mockserver, patch):
    await browser.sign_in()
    body = {'id': ID, 'expected_revision': 1, 'name': 'Клиент', **patch}

    @mockserver.json_handler('/api/v1/counterparties/update')
    def upstream(request):
        assert request.json == body
        return {'counterparty': COUNTERPARTY}

    response = await browser.post('/api/v1/counterparties/update', body)
    assert response.status == 200, response.text
    assert response.json()['counterparty']['legal_name'] is None


@pytest.mark.parametrize('legacy', [False, True])
async def test_counterparty_history_proxy_preserves_snapshots(browser, mockserver, legacy):
    await browser.sign_in()
    after = (
        {'name': 'Старое имя'}
        if legacy
        else {key: value for key, value in COUNTERPARTY.items() if key not in ('id', 'revision')}
    )
    event = {
        'id': 42,
        'action': 'update_counterparty',
        'actor': 'issuer#subject',
        'occurred_at': '2026-10-21T12:00:00+00:00',
        'after': after,
    }
    if not legacy:
        event.update(actor_name='Автор', revision=2, before={**after, 'name': 'До изменения'})
    expected = {'items': [event]}

    @mockserver.json_handler('/api/v1/counterparties/history')
    def upstream(request):
        assert request.json == {'id': ID, 'before': 100, 'limit': 20}
        assert request.headers['Authorization'].startswith('Bearer ')
        return expected

    response = await browser.post(
        '/api/v1/counterparties/history', {'id': ID, 'before': 100, 'limit': 20}
    )
    assert response.status == 200, response.text
    assert response.json() == expected


async def test_registry_lookup_proxy(browser, mockserver):
    await browser.sign_in()
    result = {
        'items': [
            {
                'name': 'Компания',
                'legal_name': None,
                'tax_id': '7707083893',
                'kpp': '773601001',
                'registration_id': None,
                'legal_address': 'Москва',
                'status': 'ACTIVE',
            }
        ]
    }

    @mockserver.json_handler('/api/v1/counterparties/lookup')
    def upstream(request):
        assert request.json == {'tax_id': '7707083893'}
        assert request.headers['Authorization'].startswith('Bearer ')
        return result

    response = await browser.post('/api/v1/counterparties/lookup', {'tax_id': '7707083893'})
    assert response.status == 200, response.text
    assert response.json() == result


@pytest.mark.parametrize(
    'claims,expected',
    [
        (
            {
                'family_name': 'Иванов',
                'given_name': 'Иван',
                'middle_name': 'Иванович',
                'name': 'Иван Иванович Иванов',
            },
            'Иванов Иван Иванович',
        ),
        ({'name': 'Анна Петрова'}, 'Анна Петрова'),
        ({}, 'author'),
    ],
)
async def test_session_uses_profile_name(browser, oidc, claims, expected):
    oidc['claims'].update(claims)
    session = await browser.sign_in()
    assert session['display_name'] == expected


async def test_registry_query_and_cursor_are_forwarded(browser, mockserver):
    await browser.sign_in()
    cursor = {'id': ID, 'value': 'mscu1234567', 'scope': 'a' * 64}
    body = {
        'limit': 10,
        'sort': 'container_number',
        'descending': True,
        'cursor': cursor,
        'filters': [
            {'field': 'container_number', 'query': '123'},
            {'field': 'type', 'values': ['container', 'air']},
            {'field': 'container_type', 'values': ['40HC']},
            {'field': 'hazardous', 'values': ['false']},
        ],
    }

    @mockserver.json_handler('/api/v1/transportations/list')
    def upstream(request):
        assert request.json == body
        return {'items': [], 'next_cursor': cursor}

    response = await browser.post('/api/v1/transportations/list', body)
    assert response.status == 200
    assert response.json()['next_cursor'] == cursor


@pytest.mark.parametrize(
    'path,body,cursor',
    [
        (
            'shipments',
            {
                'sort': 'shipment_date',
                'descending': True,
                'filters': [
                    {'field': 'number', 'query': 'S-'},
                    {'field': 'cargo_description', 'query': 'сталь'},
                    {'field': 'status', 'values': ['Новая']},
                    {
                        'field': 'shipment_date',
                        'date_from': '2026-01-01',
                        'date_to': '2026-01-31',
                    },
                ],
            },
            {'id': ID, 'scope': 'a' * 64, 'date_value': '2026-01-15'},
        ),
        (
            'counterparties',
            {
                'sort': 'name',
                'descending': False,
                'filters': [
                    {'field': 'id', 'values': [ID]},
                    {'field': 'search', 'query': 'alpha'},
                    {'field': 'roles', 'values': ['customer']},
                    {'field': 'country_code', 'values': ['RU']},
                ],
            },
            {'id': ID, 'scope': 'a' * 64, 'value': 'alpha'},
        ),
    ],
)
async def test_registry_filters_and_typed_cursors(browser, mockserver, path, body, cursor):
    await browser.sign_in()
    body = {**body, 'limit': 10, 'cursor': cursor}

    @mockserver.json_handler('/api/v1/' + path + '/list')
    def upstream(request):
        assert request.json == body
        return {'items': [], 'next_cursor': cursor}

    response = await browser.post('/api/v1/' + path + '/list', body)
    assert response.status == 200
    assert response.json()['next_cursor'] == cursor


@pytest.mark.parametrize('service',['shipments','transportations','counterparties','contracts','financial-labels'])
async def test_registry_filters_and_metadata_are_proxied(browser,mockserver,service):
    await browser.sign_in()
    path=f'/api/v1/{service}/filter-values'
    field='name' if service in ('counterparties','financial-labels') else 'number' if service in ('shipments','contracts') else 'container_number'
    result={'fields':[{'id':field,'label':'Номер','kind':'values','sortable':True,'case_insensitive':True}], 'values':[{'value':'abc2','label':'ABC2'}],'next_after':'abc2'}
    @mockserver.json_handler(path)
    def upstream(request):
        assert request.json['registry']['filters']==[{'field':field,'values':['ABC2','ABC10']}]
        assert request.json['candidates']==['ABC2','ABC10']
        return result
    response=await browser.post(path,{'registry':{'filters':[{'field':field,'values':['ABC2','ABC10']}]},'field':field,'candidates':['ABC2','ABC10']})
    assert response.status==200,response.text
    assert response.json()==result


async def test_route_filter_groups_and_metadata(browser, mockserver):
    await browser.sign_in()
    groups=[{'filters':[{'field':'mode','values':['rail']},{'field':'destination_station','values':[ID]}]}]
    @mockserver.json_handler('/api/v1/transportations/list')
    def listing(request):
        assert request.json['route_filters']==groups
        return {'items':[]}
    response=await browser.post('/api/v1/transportations/list',{'route_filters':groups})
    assert response.status==200
    @mockserver.json_handler('/api/v1/transportations/filter-values')
    def values(request):
        assert request.json['registry']['route_filters']==groups
        assert request.json['group']==0
        assert request.json['scope']=='route'
        return {'fields':[{'id':'destination_station','label':'Станция назначения','kind':'values','sortable':False,'case_insensitive':False,'lookup':'route_locations'}], 'values':[{'value':ID,'label':'Москва'}]}
    response=await browser.post('/api/v1/transportations/filter-values',{'scope':'route','registry':{'route_filters':groups},'group':0,'field':'destination_station'})
    assert response.status==200
    assert response.json()['values']==[{'value':ID,'label':'Москва'}]


async def test_contract_registry_dates_and_cursor(browser,mockserver):
    await browser.sign_in()
    cursor={'id':ID,'scope':'a'*64,'date_value':'2026-10-30'}
    body={'sort':'valid_until','descending':True,'limit':10,'cursor':cursor,'filters':[
        {'field':'own_company_id','values':[ID]},
        {'field':'contract_date','date_from':'2026-10-01','date_to':'2026-10-31'},
        {'field':'valid_until','empty':False},
    ]}
    @mockserver.json_handler('/api/v1/contracts/list')
    def upstream(request):
        assert request.json == body
        return {'items':[],'next_cursor':cursor}
    response=await browser.post('/api/v1/contracts/list',body)
    assert response.status==200,response.text
    assert response.json()['next_cursor']==cursor

async def test_counterparty_settlements_proxy(browser, mockserver):
    await browser.sign_in()
    body = {'counterparty_id': CUSTOMER, 'own_company_id': ID, 'currency': 'RUB'}
    result = {'totals': [{
        'currency': 'RUB', 'receivable_minor': '9007199254740993', 'payable_minor': '0',
        'overdue_receivable_minor': '100', 'overdue_payable_minor': '0',
        'unallocated_receipts_minor': '200', 'unallocated_disbursements_minor': '0',
    }]}

    @mockserver.json_handler('/api/v1/counterparty-settlements/get')
    def upstream(request):
        assert request.json == body
        return result

    response = await browser.post('/api/v1/counterparty-settlements/get', body)
    assert response.status == 200
    assert response.json() == result
