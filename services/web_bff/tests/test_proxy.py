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
    'pending_quantity': 0,
    'available_quantity': 5,
}
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


@pytest.mark.parametrize(
    'path,body,result,status',
    [
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
            {'item': ITEM},
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
        ('shipment-items/list', {'shipment_id': ID}, {'items': [ITEM]}, 200),
        (
            'shipment-items/update',
            {'item_id': ID, 'expected_revision': 1, 'planned_quantity': 5},
            {'item': ITEM},
            200,
        ),
        (
            'transportation-operations/create',
            {'item_id': ID, 'expected_revision': 1, 'quantity': 3, 'idempotency_key': ID},
            {'operation': OPERATION},
            202,
        ),
        ('transportation-operations/get', {'operation_id': ID}, {'operation': OPERATION}, 200),
        ('transportations/get', {'transportation_id': ID}, {'transportation': TRANSPORTATION}, 200),
        ('transportations/list', {'shipment_id': ID}, {'items': [TRANSPORTATION]}, 200),
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
        assert request.json == (
            {'limit': limit, 'query': '', 'status': 'active'}
            if path == 'counterparties'
            else {'limit': limit}
        )
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
