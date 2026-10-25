"""История сохраняет снимки и изолирована так же, как карточка контрагента."""

import json
from uuid import uuid4

import pytest

from conftest import ORGANIZATION


def snapshot(counterparty):
    return {key: value for key, value in counterparty.items() if key not in ('id', 'revision')}


async def create_card(api, name='Клиент'):
    response = await api.post(
        '/api/v1/counterparties/create', {'id': str(uuid4()), 'name': name}, expected=201
    )
    return response['counterparty']


async def test_history_records_actor_and_snapshots(api):
    created = await create_card(api)
    updated_response = await api.post(
        '/api/v1/counterparties/update',
        {
            'id': created['id'],
            'expected_revision': 1,
            'name': 'Новое имя',
            'legal_name': 'ООО Новое имя',
            'roles': ['customer'],
            'contacts': [{'name': 'Анна', 'email': 'anna@example.test'}],
        },
    )
    updated = updated_response['counterparty']
    archived_response = await api.post(
        '/api/v1/counterparties/set-archived',
        {
            'id': created['id'],
            'expected_revision': 2,
            'archived': True,
        },
    )
    archived = archived_response['counterparty']
    restored_response = await api.post(
        '/api/v1/counterparties/set-archived',
        {
            'id': created['id'],
            'expected_revision': 3,
            'archived': False,
        },
    )
    restored = restored_response['counterparty']

    response = await api.post(
        '/api/v1/counterparties/history', {'id': created['id']}, roles=['counterparties.read']
    )
    events = response['items']
    assert [event['action'] for event in events] == [
        'restore_counterparty',
        'archive_counterparty',
        'update_counterparty',
        'create_counterparty',
    ]
    assert [event['revision'] for event in events] == [4, 3, 2, 1]
    assert [event['id'] for event in events] == sorted(
        [event['id'] for event in events], reverse=True
    )
    for event in events:
        assert event['actor'].endswith('#author')
        assert event['actor_name'] == 'author'
        assert event['occurred_at'].endswith('+00:00')
    assert events[0]['before'] == snapshot(archived)
    assert events[0]['after'] == snapshot(restored)
    assert events[1]['before'] == snapshot(updated)
    assert events[1]['after'] == snapshot(archived)
    assert events[2]['before'] == snapshot(created)
    assert events[2]['after'] == snapshot(updated)
    assert events[3]['after'] == snapshot(created)
    assert 'before' not in events[3]


async def test_history_cursor_keeps_previous_snapshot_outside_page(api):
    created = await create_card(api)
    updated = (
        await api.post(
            '/api/v1/counterparties/update',
            {
                'id': created['id'],
                'expected_revision': 1,
                'name': 'Обновлённый клиент',
            },
        )
    )['counterparty']
    first = (await api.post('/api/v1/counterparties/history', {'id': created['id'], 'limit': 1}))[
        'items'
    ]
    assert len(first) == 1
    assert first[0]['after'] == snapshot(updated)
    assert first[0]['before'] == snapshot(created)
    second = (
        await api.post(
            '/api/v1/counterparties/history',
            {
                'id': created['id'],
                'limit': 1,
                'before': first[0]['id'],
            },
        )
    )['items']
    assert len(second) == 1
    assert second[0]['action'] == 'create_counterparty'
    assert second[0]['id'] < first[0]['id']
    assert (
        await api.post(
            '/api/v1/counterparties/history',
            {
                'id': created['id'],
                'before': second[0]['id'],
            },
        )
    )['items'] == []


async def test_history_does_not_leak_other_organizations_or_entities(api):
    card = await create_card(api)
    other_card = await create_card(api, name='Другой контрагент')
    response = await api.post('/api/v1/counterparties/history', {'id': card['id']})
    assert [event['after']['name'] for event in response['items']] == ['Клиент']
    await api.post('/api/v1/counterparties/history', {'id': card['id']}, user='other', expected=404)
    await api.post(
        '/api/v1/counterparties/history',
        {'id': other_card['id']},
        roles=['counterparties.write'],
        expected=403,
    )
    await api.post('/api/v1/counterparties/history', {'id': str(uuid4())}, expected=404)


@pytest.mark.parametrize(
    'patch',
    [{'limit': 0}, {'limit': 101}, {'before': 0}, {'id': 'bad-id'}, {'actor_name': 'spoofed'}],
)
async def test_history_rejects_invalid_parameters(api, patch):
    await api.post('/api/v1/counterparties/history', {'id': str(uuid4()), **patch}, expected=400)


async def test_failed_update_and_replayed_create_do_not_add_history(api):
    created = await create_card(api)
    await api.post(
        '/api/v1/counterparties/create',
        {'id': created['id'], 'name': created['name']},
        expected=201,
    )
    await api.post(
        '/api/v1/counterparties/update',
        {
            'id': created['id'],
            'name': 'Не должно сохраниться',
            'expected_revision': 2,
        },
        expected=409,
    )
    events = (await api.post('/api/v1/counterparties/history', {'id': created['id']}))['items']
    assert len(events) == 1
    assert events[0]['after'] == snapshot(created)


async def test_legacy_history_preserves_unknown_fields_and_infers_adjacent_snapshot(api, pgsql):
    created = await create_card(api)
    # Имитируем старые снимки: только название и более поздние полные записи без before.
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute(
            "UPDATE business_audit SET document=%s::jsonb WHERE entity_id=%s",
            (
                json.dumps({'result': {'id': created['id'], 'name': 'Старое имя'}}),
                created['id'],
            ),
        )
        for revision in (2, 3, 5):
            cursor.execute(
                """INSERT INTO business_audit (organization_id, actor, action, entity_id, document)
                VALUES (%s, 'legacy#user', 'update_counterparty', %s, %s::jsonb)""",
                (
                    ORGANIZATION,
                    created['id'],
                    json.dumps(
                        {
                            'result': {
                                **created,
                                'revision': revision,
                                'name': f'Редакция {revision}',
                            }
                        }
                    ),
                ),
            )
    events = (await api.post('/api/v1/counterparties/history', {'id': created['id']}))['items']
    assert 'before' not in events[0]  # Между редакциями 3 и 5 есть пропуск.
    assert events[1]['before']['name'] == 'Редакция 2'
    assert 'actor_name' not in events[1]
    assert 'before' not in events[2]  # У исходного снимка неизвестна редакция.
    assert events[3]['after'] == {'name': 'Старое имя'}
    assert 'revision' not in events[3]
    # Даже при лимите 1 предыдущий снимок берётся из более раннего события.
    page = (
        await api.post(
            '/api/v1/counterparties/history',
            {
                'id': created['id'],
                'before': events[0]['id'],
                'limit': 1,
            },
        )
    )['items']
    assert page[0]['before'] == events[1]['before']
