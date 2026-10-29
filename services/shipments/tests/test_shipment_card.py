"""Карточка поставки: сохранение, номера, календарные даты и общие справочники."""

import asyncio
from uuid import uuid4
import pytest


def creation(**fields):
    return {'customer_id': str(uuid4()), 'idempotency_key': str(uuid4()), **fields}


async def test_card_roundtrip_and_patch_preserves_fields(api):
    body = creation(
        number='LCB-1/2026',
        shipment_date='2026-10-22',
        recipient_id=str(uuid4()),
        logist='logist-author',
        cargo_description='  Запасные части  ',
        status='Новая',
        incoterms='FOB',
        carriage_terms='FILO',
        loading='FCL',
        delivery_city='Владивосток',
        delivery_country='RU',
        delivery_address='Терминал',
        route_segments=[{'id': str(uuid4()), 'type': mode} for mode in ['sea', 'rail', 'pickup']],
    )
    created = (await api.post('/api/v1/shipments/create', body, expected=201))['shipment']
    assert created['number'] == 'LCB-1/2026'
    assert created['cargo_description'] == 'Запасные части'
    for field in (
        'shipment_date',
        'recipient_id',
        'logist',
        'status',
        'incoterms',
        'carriage_terms',
        'loading',
        'delivery_city',
        'delivery_country',
        'delivery_address',
        'route_segments',
    ):
        assert created[field] == body[field]
    assert (await api.post('/api/v1/shipments/get', {'shipment_id': created['id']}))[
        'shipment'
    ] == created
    changed = (
        await api.post(
            '/api/v1/shipments/update',
            {
                'shipment_id': created['id'],
                'expected_revision': 1,
                'reference': 'Изменено',
            },
        )
    )['shipment']
    assert changed['revision'] == 2
    assert changed['recipient_id'] == body['recipient_id']
    assert changed['route_segments'] == body['route_segments']
    cleared = (
        await api.post(
            '/api/v1/shipments/update',
            {
                'shipment_id': created['id'],
                'expected_revision': 2,
                'reference': '',
                'recipient_id': None,
                'cargo_description': None,
                'logist': None,
            },
        )
    )['shipment']
    assert cleared['recipient_id'] is None
    assert cleared['logist'] is None
    assert cleared['cargo_description'] is None


@pytest.mark.parametrize(
    'date', ['1111-11-11', '2025-02-29', '2026-04-31', '2026-13-01', 'not-a-date']
)
async def test_invalid_shipment_date(api, date):
    await api.post('/api/v1/shipments/create', creation(shipment_date=date), expected=400)


@pytest.mark.parametrize('date', ['1900-01-01', '2000-02-29', '2099-12-31'])
async def test_valid_shipment_dates_include_planned_future_dates(api, date):
    shipment = (
        await api.post('/api/v1/shipments/create', creation(shipment_date=date), expected=201)
    )['shipment']
    assert shipment['shipment_date'] == date


async def test_number_unique_within_organization(api):
    body = creation(number='CUSTOM-1')
    await api.post('/api/v1/shipments/create', body, expected=201)
    await api.post(
        '/api/v1/shipments/create', {**body, 'idempotency_key': str(uuid4())}, expected=409
    )
    await api.post(
        '/api/v1/shipments/create',
        {**body, 'idempotency_key': str(uuid4())},
        user='other',
        expected=201,
    )


async def test_concurrent_automatic_numbers_and_replay(api):
    bodies = [creation() for _ in range(8)]
    shipments = await asyncio.gather(
        *(api.post('/api/v1/shipments/create', body, expected=201) for body in bodies)
    )
    assert len({value['shipment']['number'] for value in shipments}) == 8
    assert await api.post('/api/v1/shipments/create', bodies[0], expected=201) == shipments[0]


@pytest.mark.parametrize(
    'field,value',
    [
        ('incoterms', 'WRONG'),
        ('loading', 'WRONG'),
        ('carriage_terms', 'WRONG'),
        ('status', 'Unknown'),
        ('route_segments', [{'id': str(uuid4()), 'type': 'unknown'}]),
    ],
)
async def test_unknown_catalog_values(api, field, value):
    await api.post('/api/v1/shipments/create', creation(**{field: value}), expected=400)


async def test_catalog_is_source_for_validation(api, pgsql):
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute(
            "INSERT INTO shipment_dictionary_values VALUES ('loading','TEST','Тестовая загрузка',99,TRUE)"
        )
    dictionaries = await api.post('/api/v1/shipments/dictionaries', {})
    assert {'code': 'TEST', 'name': 'Тестовая загрузка'} in dictionaries['loading']
    shipment = (await api.post('/api/v1/shipments/create', creation(loading='TEST'), expected=201))[
        'shipment'
    ]
    assert shipment['loading'] == 'TEST'


async def test_cargo_suggestions_are_isolated_and_removed_values_stay_hidden(api):
    body = creation(cargo_description='Комплектующие')
    await api.post('/api/v1/shipments/create', body, expected=201)
    assert (
        await api.post(
            '/api/v1/shipments/cargo-suggestions',
            {'customer_id': body['customer_id'], 'query': 'компл'},
        )
    )['items'] == ['Комплектующие']
    assert (
        await api.post(
            '/api/v1/shipments/cargo-suggestions',
            {'customer_id': body['customer_id']},
            user='other',
        )
    )['items'] == []
    assert (
        await api.post(
            '/api/v1/shipments/cargo-suggestions/remove',
            {'customer_id': body['customer_id'], 'value': 'Комплектующие'},
        )
    )['deleted']
    await api.post(
        '/api/v1/shipments/create', {**body, 'idempotency_key': str(uuid4())}, expected=201
    )
    assert (
        await api.post('/api/v1/shipments/cargo-suggestions', {'customer_id': body['customer_id']})
    )['items'] == []


def settings_value(**overrides):
    return {
        'number_template': 'S-{sequence}',
        'reset_number_yearly': False,
        'statuses': ['Новая'],
        'route_types': ['sea', 'rail', 'pickup'],
        'planned_containers': True,
        'container_types': ['20DC', '40DC', '40HC'],
        **overrides,
    }


@pytest.mark.config(
    SHIPMENT_SETTINGS={
        'clauses': [
            {
                'title': 'Другая организация',
                'predicates': [
                    {
                        'field': 'organization_id',
                        'type': 'eq',
                        'value': '22222222-2222-4222-8222-222222222222',
                    }
                ],
                'value': settings_value(
                    number_template='LED{sequence}/{yyyy}',
                    reset_number_yearly=True,
                    statuses=['Черновик'],
                ),
            }
        ],
        'default': settings_value(number_template='LCB{sequence}/{yyyy}'),
    }
)
async def test_organization_settings_and_yearly_numbers(api):
    first = (
        await api.post(
            '/api/v1/shipments/create', creation(shipment_date='2026-01-02'), expected=201
        )
    )['shipment']
    assert first['number'] == 'LCB1/2026'
    other = (
        await api.post(
            '/api/v1/shipments/create',
            creation(shipment_date='2026-01-02'),
            user='other',
            expected=201,
        )
    )['shipment']
    assert other['number'] == 'LED1/2026'
    assert other['status'] == 'Черновик'
    next_year = (
        await api.post(
            '/api/v1/shipments/create',
            creation(shipment_date='2027-01-02'),
            user='other',
            expected=201,
        )
    )['shipment']
    assert next_year['number'] == 'LED1/2027'


@pytest.mark.config(
    SHIPMENT_SETTINGS={
        'clauses': [],
        'default': settings_value(planned_containers=False),
    }
)
async def test_disabled_planned_containers_are_rejected_by_server(api):
    shipment = (await api.post('/api/v1/shipments/create', creation(), expected=201))['shipment']
    settings = await api.post('/api/v1/shipments/settings', {})
    assert settings['planned_containers'] is False
    await api.post(
        '/api/v1/shipment-items/add',
        {
            'shipment_id': shipment['id'],
            'expected_revision': shipment['revision'],
            'container_type_code': '40HC',
            'container_ownership': 'COC',
            'planned_quantity': 1,
            'idempotency_key': str(uuid4()),
        },
        expected=400,
    )


async def test_route_segment_ids_are_preserved_and_duplicates_rejected(api):
    segments = [{'id': str(uuid4()), 'type': 'sea'}, {'id': str(uuid4()), 'type': 'sea'}]
    shipment = (
        await api.post('/api/v1/shipments/create', creation(route_segments=segments), expected=201)
    )['shipment']
    changed = (
        await api.post(
            '/api/v1/shipments/update',
            {
                'shipment_id': shipment['id'],
                'expected_revision': 1,
                'reference': '',
                'route_segments': list(reversed(segments)),
            },
        )
    )['shipment']
    assert changed['route_segments'] == list(reversed(segments))
    await api.post(
        '/api/v1/shipments/create',
        creation(route_segments=[segments[0], segments[0]]),
        expected=400,
    )


async def test_cargo_suggestions_limit_and_server_filter(api, pgsql):
    from conftest import ORGANIZATION

    body = creation()

    with pgsql['db_1'].cursor() as cursor:
        cursor.execute(
            '''
            INSERT INTO customer_cargo_suggestions (organization_id, customer_id, value, search_value)
            SELECT %s::UUID, %s::UUID, 'Описание ' || index, 'описание ' || index
            FROM generate_series(1, 1000) AS index
        ''',
            (ORGANIZATION, body['customer_id']),
        )
    assert (
        len(
            (
                await api.post(
                    '/api/v1/shipments/cargo-suggestions', {'customer_id': body['customer_id']}
                )
            )['items']
        )
        == 20
    )
    assert (
        await api.post(
            '/api/v1/shipments/cargo-suggestions',
            {'customer_id': body['customer_id'], 'query': 'Описание 1000'},
        )
    )['items'] == ['Описание 1000']


@pytest.mark.parametrize('subject', ['missing-employee', 'foreign-employee'])
async def test_logist_must_be_active_member(api, subject):
    result = await api.post('/api/v1/shipments/create', creation(logist=subject), expected=400)
    assert result['error'] == 'invalid_logist'


async def test_customer_logist_can_be_overridden_or_cleared(api, mockserver):
    @mockserver.json_handler('/api/v1/counterparties/get')
    def customer_record(request):
        return {
            'counterparty': {
                'id': request.json['id'],
                'name': 'Клиент',
                'revision': 1,
                'archived': False,
                'own_company': False,
                'legal_name': None,
                'country_code': None,
                'tax_id': None,
                'registration_id': None,
                'legal_address': None,
                'roles': ['customer'],
                'contacts': [],
                'logist': 'logist-default',
            }
        }

    customer = '33333333-3333-4333-8333-333333333333'
    settings = await api.post('/api/v1/shipments/settings', {'customer_id': customer})
    assert settings['customer_logist'] == 'logist-default'
    for fields, expected in [
        ({}, 'logist-default'),
        ({'logist': 'logist-author'}, 'logist-author'),
        ({'logist': None}, None),
    ]:
        result = (
            await api.post(
                '/api/v1/shipments/create', creation(customer_id=customer, **fields), expected=201
            )
        )['shipment']
        assert result['logist'] == expected


async def test_create_replay_survives_employee_directory_outage(api, mockserver):
    body = creation(logist='logist-author')
    created = await api.post('/api/v1/shipments/create', body, expected=201)

    @mockserver.json_handler('/api/organizations/employees/list')
    def unavailable(request):
        return mockserver.make_response(
            status=503, json={'error': 'unavailable', 'message': 'Unavailable'}
        )

    assert await api.post('/api/v1/shipments/create', body, expected=201) == created
    assert unavailable.times_called == 0


async def test_employee_directory_outage_is_explicit_and_does_not_create_shipment(
    api, mockserver, pgsql
):
    @mockserver.json_handler('/api/organizations/employees/list')
    def unavailable(request):
        return mockserver.make_response(
            status=503, json={'error': 'unavailable', 'message': 'Unavailable'}
        )

    result = await api.post(
        '/api/v1/shipments/create', creation(logist='logist-author'), expected=503
    )
    assert result['error'] == 'employee_directory_unavailable'
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT COUNT(*) FROM shipments')
        assert cursor.fetchone()[0] == 0


async def test_cargo_suggestions_are_scoped_to_customer(api):
    first = creation(cargo_description='Оборудование')
    second = creation(cargo_description='Оборудование')
    await api.post('/api/v1/shipments/create', first, expected=201)
    await api.post('/api/v1/shipments/create', second, expected=201)
    await api.post(
        '/api/v1/shipments/cargo-suggestions/remove',
        {'customer_id': first['customer_id'], 'value': 'Оборудование'},
    )
    assert (
        await api.post('/api/v1/shipments/cargo-suggestions', {'customer_id': first['customer_id']})
    )['items'] == []
    assert (
        await api.post(
            '/api/v1/shipments/cargo-suggestions', {'customer_id': second['customer_id']}
        )
    )['items'] == ['Оборудование']


async def test_settings_without_customer_have_no_assigned_logist(api):
    settings = await api.post('/api/v1/shipments/settings', {})
    assert settings['customer_logist'] is None
    assert 'default_logist' not in settings
