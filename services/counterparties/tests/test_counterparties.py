"""Имена контрагентов и доступ проверяются с настоящим PostgreSQL."""

import asyncio
from uuid import uuid4
import pytest
from conftest import ORGANIZATION


async def test_create_replay_and_organization_isolation(api, pgsql):
    identifier = str(uuid4())
    body = {'id': identifier, 'name': '  ООО Тестовый клиент  '}
    created = await api.post('/api/v1/counterparties/create', body, expected=201)
    assert created['counterparty']['id'] == identifier
    assert created['counterparty']['name'] == 'ООО Тестовый клиент'
    assert created['counterparty']['revision'] == 1
    assert created['counterparty']['roles'] == []
    assert created['counterparty']['contacts'] == []
    assert created['counterparty']['archived'] is False
    assert await api.post('/api/v1/counterparties/create', body, expected=201) == created
    await api.post('/api/v1/counterparties/create', {**body, 'name': 'Другой'}, expected=409)
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT count(*) FROM business_audit WHERE entity_id = %s', (identifier,))
        assert cursor.fetchone()[0] == 1
    listed = await api.post('/api/v1/counterparties/list', {})
    assert listed['items'] == [created['counterparty']]
    assert (await api.post('/api/v1/counterparties/list', {}, user='other'))['items'] == []
    await api.post(
        '/api/v1/counterparties/create', body, roles=['counterparties.read'], expected=403
    )
    await api.post('/api/v1/counterparties/list', {}, roles=['shipments.read'], expected=403)


@pytest.mark.parametrize('name', ['', '   ', '\t\r\n', ' \t\r\n '])
async def test_blank_name(api, name):
    await api.post(
        '/api/v1/counterparties/create', {'id': str(uuid4()), 'name': name}, expected=400
    )


@pytest.mark.parametrize('name', ['Клиент', '  Клиент  ', '\tКлиент\r\n', 'Клиент с пробелами'])
async def test_trim_name(api, name):
    result = await api.post(
        '/api/v1/counterparties/create', {'id': str(uuid4()), 'name': name}, expected=201
    )
    assert result['counterparty']['name'] == name.strip()


async def test_concurrent_replay(api, pgsql):
    body = {'id': str(uuid4()), 'name': 'Клиент'}
    first, second = await asyncio.gather(
        api.post('/api/v1/counterparties/create', body, expected=201),
        api.post('/api/v1/counterparties/create', body, expected=201),
    )
    assert first == second
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT count(*) FROM business_audit WHERE entity_id = %s', (body['id'],))
        assert cursor.fetchone()[0] == 1


def details(**patch):
    return {
        'id': str(uuid4()),
        'name': '  ООО Логистика  ',
        'legal_name': ' Общество Логистика ',
        'country_code': 'ru',
        'tax_id': ' 7701-234567 ',
        'registration_id': ' 1234567890123 ',
        'legal_address': ' Москва ',
        'roles': ['carrier', 'customer'],
        'contacts': [
            {
                'name': ' Анна ',
                'position': ' Логист ',
                'email': ' anna@example.test ',
                'phone': ' +7 900 000-00-00 ',
            }
        ],
        **patch,
    }


async def test_card_edit_archive_and_links(api, pgsql):
    created = (await api.post('/api/v1/counterparties/create', details(), expected=201))[
        'counterparty'
    ]
    identifier = created['id']
    assert created['tax_id'] == '7701-234567'
    assert created['country_code'] == 'RU'
    assert created['contacts'][0]['name'] == 'Анна'
    fetched = await api.post('/api/v1/counterparties/get', {'id': identifier})
    assert fetched['counterparty'] == created
    await api.post('/api/v1/counterparties/get', {'id': identifier}, user='other', expected=404)
    update = details(id=identifier, name='Новое имя', expected_revision=1)
    changed = (await api.post('/api/v1/counterparties/update', update))['counterparty']
    assert changed['name'] == 'Новое имя'
    assert changed['revision'] == 2
    await api.post('/api/v1/counterparties/update', update, expected=409)
    archived = (
        await api.post(
            '/api/v1/counterparties/set-archived',
            {'id': identifier, 'expected_revision': 2, 'archived': True},
        )
    )['counterparty']
    assert archived['archived'] is True
    assert archived['revision'] == 3
    assert (await api.post('/api/v1/counterparties/list', {}))['items'] == []
    assert (await api.post('/api/v1/counterparties/list', {'status': 'archived'}))['items'] == [
        archived
    ]
    assert (await api.post('/api/v1/counterparties/get', {'id': identifier}))[
        'counterparty'
    ] == archived
    await api.post(
        '/api/v1/counterparties/update', {**update, 'expected_revision': 3}, expected=409
    )
    restored = (
        await api.post(
            '/api/v1/counterparties/set-archived',
            {'id': identifier, 'expected_revision': 3, 'archived': False},
        )
    )['counterparty']
    assert restored['revision'] == 4
    assert restored['archived'] is False
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute(
            'SELECT action FROM business_audit WHERE entity_id=%s ORDER BY id', (identifier,)
        )
        assert [row[0] for row in cursor.fetchall()] == [
            'create_counterparty',
            'update_counterparty',
            'archive_counterparty',
            'restore_counterparty',
        ]


async def test_requisites_duplicates_are_scoped_to_organization_and_country(api):
    first = (await api.post('/api/v1/counterparties/create', details(), expected=201))[
        'counterparty'
    ]
    error = await api.post(
        '/api/v1/counterparties/create', details(tax_id='7701-234567'), expected=409
    )
    assert error['error'] == 'duplicate_requisites'
    await api.post('/api/v1/counterparties/create', details(), user='other', expected=201)
    await api.post('/api/v1/counterparties/create', details(country_code='CN'), expected=201)
    await api.post(
        '/api/v1/counterparties/set-archived',
        {'id': first['id'], 'expected_revision': 1, 'archived': True},
    )
    await api.post('/api/v1/counterparties/create', details(), expected=409)


async def test_concurrent_duplicate_requisites(api):
    async def create(body):
        return await api.post('/api/v1/counterparties/create', body, expected=(201, 409))

    # Ответы проверяем через реальный клиент без предопределённого порядка победителя.
    first, second = details(), details()
    result = await asyncio.gather(create(first), create(second))
    assert sum('counterparty' in value for value in result) == 1
    assert sum(value.get('error') == 'duplicate_requisites' for value in result) == 1


async def test_search_role_status_and_cursor(api):
    a = (
        await api.post(
            '/api/v1/counterparties/create', details(tax_id='111', name='Альфа'), expected=201
        )
    )['counterparty']
    b = (
        await api.post(
            '/api/v1/counterparties/create',
            details(tax_id='222', name='Бета', roles=['agent']),
            expected=201,
        )
    )['counterparty']
    assert [
        item['id']
        for item in (await api.post('/api/v1/counterparties/list', {'query': '111'}))['items']
    ] == [a['id']]
    assert [
        item['id']
        for item in (await api.post('/api/v1/counterparties/list', {'role': 'agent'}))['items']
    ] == [b['id']]
    assert (await api.post('/api/v1/counterparties/list', {'query': '%'}))['items'] == []
    page = (await api.post('/api/v1/counterparties/list', {'limit': 1}))['items']
    following = (
        await api.post('/api/v1/counterparties/list', {'after': page[0]['id'], 'limit': 1})
    )['items']
    assert {page[0]['id'], following[0]['id']} == {a['id'], b['id']}


@pytest.mark.parametrize(
    'patch',
    [
        {'country_code': '', 'tax_id': '123'},
        {'country_code': 'Russia'},
        {'roles': ['invalid']},
        {'roles': None},
        {'contacts': None},
        {'roles': ['customer', 'customer']},
        {'contacts': [{'name': '   '}]},
        {'contacts': [{'name': 'Анна', 'email': 'no-email'}]},
    ],
)
async def test_invalid_details(api, patch):
    await api.post('/api/v1/counterparties/create', details(**patch), expected=400)


@pytest.mark.parametrize(
    'path,body',
    [
        ('get', {'id': str(uuid4())}),
        ('update', {'id': str(uuid4()), 'name': 'Клиент', 'expected_revision': 1}),
        ('set-archived', {'id': str(uuid4()), 'expected_revision': 1, 'archived': True}),
    ],
)
async def test_card_missing_and_permissions(api, path, body):
    await api.post('/api/v1/counterparties/' + path, body, expected=404)
    await api.post('/api/v1/counterparties/' + path, body, roles=['shipments.read'], expected=403)


async def test_uuid_normalization_and_update_duplicate_rolls_back(api):
    first = details(id=str(uuid4()).upper(), tax_id='111')
    created = (await api.post('/api/v1/counterparties/create', first, expected=201))['counterparty']
    assert created['id'] == first['id'].lower()
    assert (await api.post('/api/v1/counterparties/create', first, expected=201))[
        'counterparty'
    ] == created
    await api.post('/api/v1/counterparties/create', details(tax_id='222'), expected=201)
    await api.post(
        '/api/v1/counterparties/update',
        details(id=created['id'], tax_id='222', name='Не должно сохраниться', expected_revision=1),
        expected=409,
    )
    assert (await api.post('/api/v1/counterparties/get', {'id': created['id']}))[
        'counterparty'
    ] == created


async def test_nullable_requisites_and_partial_update(api, pgsql):
    created = (await api.post('/api/v1/counterparties/create', details(), expected=201))[
        'counterparty'
    ]
    base = {'id': created['id'], 'name': created['name'], 'expected_revision': 1}
    kept = (await api.post('/api/v1/counterparties/update', base))['counterparty']
    assert kept['revision'] == 2
    for field, expected in created.items():
        if field != 'revision':
            assert kept[field] == expected, field
    cleared = (
        await api.post(
            '/api/v1/counterparties/update',
            {
                **base,
                'expected_revision': 2,
                'legal_name': None,
                'tax_id': None,
                'country_code': None,
                'registration_id': None,
                'legal_address': None,
                'roles': [],
                'contacts': [],
            },
        )
    )['counterparty']
    for field in ['legal_name', 'tax_id', 'country_code', 'registration_id', 'legal_address']:
        assert cleared[field] is None
    assert cleared['roles'] == []
    assert cleared['contacts'] == []
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute(
            'SELECT legal_name, tax_id, country_code, registration_id, legal_address FROM counterparties WHERE id=%s',
            (created['id'],),
        )
        assert cursor.fetchone() == (None, None, None, None, None)
    listed = await api.post('/api/v1/counterparties/list', {'query': created['name']})
    assert listed['items'] == [cleared]
    replaced = (
        await api.post(
            '/api/v1/counterparties/update',
            {
                **base,
                'expected_revision': 3,
                'legal_name': ' Новое юрлицо ',
            },
        )
    )['counterparty']
    assert replaced['legal_name'] == 'Новое юрлицо'
    assert replaced['tax_id'] is None


@pytest.mark.parametrize('empty', [None, '', '   '])
async def test_absent_requisites_are_null(api, pgsql, empty):
    created = (
        await api.post(
            '/api/v1/counterparties/create',
            {
                'id': str(uuid4()),
                'name': 'Без реквизитов',
                'legal_name': empty,
                'contacts': [{'name': 'Анна', 'email': empty}],
            },
            expected=201,
        )
    )['counterparty']
    assert created['legal_name'] is None
    assert created['tax_id'] is None
    assert created['contacts'] == [{'name': 'Анна', 'email': None, 'phone': None, 'position': None}]
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute(
            'SELECT legal_name, tax_id, contacts FROM counterparties WHERE id=%s', (created['id'],)
        )
        assert cursor.fetchone() == (None, None, created['contacts'])


async def test_tax_identifier_keeps_internal_separators(api):
    created = (
        await api.post('/api/v1/counterparties/create', details(tax_id=' ab-12 34 '), expected=201)
    )['counterparty']
    assert created['tax_id'] == 'ab-12 34'


async def test_concurrent_updates_have_one_winner(api):
    created = (await api.post('/api/v1/counterparties/create', details(), expected=201))[
        'counterparty'
    ]

    async def update(name):
        return await api.post(
            '/api/v1/counterparties/update',
            {
                'id': created['id'],
                'name': name,
                'expected_revision': 1,
            },
            expected=(200, 409),
        )

    results = await asyncio.gather(update('Первое имя'), update('Второе имя'))
    successes = [value['counterparty'] for value in results if 'counterparty' in value]
    conflicts = [value for value in results if value.get('error') == 'revision_conflict']
    assert len(successes) == 1
    assert len(conflicts) == 1
    assert successes[0]['revision'] == 2
    persisted = (await api.post('/api/v1/counterparties/get', {'id': created['id']}))[
        'counterparty'
    ]
    assert persisted == successes[0]


@pytest.mark.parametrize(
    'upstream_status,payload',
    [
        (503, {'error': 'unavailable', 'message': 'Временно недоступно'}),
        (200, {'organization_id': '', 'permissions': ['counterparties.write']}),
        (200, {'organization_id': ORGANIZATION, 'permissions': []}),
    ],
)
async def test_central_access_failure_never_creates_counterparty(
    api, mockserver, pgsql, upstream_status, payload
):
    @mockserver.handler('/organizations/access')
    def unavailable(request):
        return mockserver.make_response(status=upstream_status, json=payload)

    await api.post(
        '/api/v1/counterparties/create',
        {
            'id': str(uuid4()),
            'name': 'Не должен создаться',
        },
        expected=503,
    )
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT count(*) FROM counterparties')
        assert cursor.fetchone()[0] == 0


@pytest.mark.parametrize('field,value', [
    ('email', 'anna@@example.test'), ('email', 'anna@example'),
    ('email', 'anna@.test'), ('email', 'a..b@example.test'),
    ('phone', '123'), ('phone', 'phone number'),
    ('phone', '1234567890123456'), ('phone', '+7 ((900)) 000-00-00'),
])
@pytest.mark.parametrize('operation', ['create', 'update'])
async def test_invalid_contact_does_not_save(api, field, value, operation):
    body = details(contacts=[{'name': 'Анна', field: value}])
    if operation == 'update':
        created = (await api.post('/api/v1/counterparties/create', details(), expected=201))['counterparty']
        body.update(id=created['id'], expected_revision=created['revision'])
    await api.post('/api/v1/counterparties/' + operation, body, expected=400)
    if operation == 'update':
        saved = (await api.post('/api/v1/counterparties/get', {'id': created['id']}))['counterparty']
        assert saved == created
