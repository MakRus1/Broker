"""Вход проходит через настоящую проверку JWT; бизнес-сервисы мокируются на HTTP-границе."""

from uuid import uuid4
import pytest

ID = str(uuid4())
CUSTOMER = str(uuid4())
SHIPMENT = {'id': ID, 'revision': 1, 'number': 'S-1', 'customer_id': CUSTOMER, 'reference': 'Тест'}
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
    'id': ID,
    'number': 'T-1',
    'revision': 1,
    'shipment_id': ID,
    'item_id': ID,
    'operation_id': ID,
    'container_type_code': '40HC',
    'container_ownership': 'COC',
    'route_segments': [],
}


@pytest.mark.parametrize(
    'path,body,result,status',
    [
        (
            'counterparties/create',
            {'id': ID, 'name': 'Клиент'},
            {'counterparty': {'id': ID, 'name': 'Клиент'}},
            201,
        ),
        ('counterparties/list', {}, {'items': [{'id': ID, 'name': 'Клиент'}]}, 200),
        (
            'shipments/create',
            {'customer_id': CUSTOMER, 'idempotency_key': ID},
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

    @mockserver.json_handler('/api/v1/' + path)
    def upstream(request):
        assert request.headers['Authorization'].startswith('Bearer ')
        assert 'broker_web' not in request.headers.get('Cookie', '')
        for key, value in body.items():
            assert request.json[key] == value
        return mockserver.make_response(status=status, json=result)

    response = await browser.post('/api/v1/' + path, body)
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
        assert request.json == {'limit': limit}
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
                'planned_departure_at': '2026-10-21T10:00:00Z',
                'planned_arrival_at': '2026-10-23T10:00:00Z',
                'actual_departure_at': '2026-10-21T11:00:00Z',
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
                        'planned_departure_at': '2026-10-21T10:00:00Z',
                        'planned_arrival_at': '2026-10-23T10:00:00Z',
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
                'occurred_at': '2026-10-23T10:00:00Z',
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
                'occurred_at': '2026-10-21T11:00:00Z',
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
                        'occurred_at': '2026-10-21T11:00:00Z',
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

    @mockserver.json_handler('/api/v1/' + path)
    def upstream(request):
        assert request.json == body
        assert request.headers['Authorization'].startswith('Bearer ')
        return result

    response = await browser.post('/api/v1/' + path, body)
    assert response.status == 200, response.text
    assert response.json() == result
