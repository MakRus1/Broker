"""Физическая очистка подсказок не меняет сохранённые описания поставок."""

from uuid import uuid4

import pytest


@pytest.mark.parametrize(
    'removed,age_days,survives',
    [
        (False, 179, True),
        (False, 181, False),
        (True, 29, True),
        (True, 31, False),
    ],
)
async def test_retention(service_client, pgsql, removed, age_days, survives):
    value = f'Срок хранения {uuid4()}'
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute(
            '''
            INSERT INTO cargo_suggestions (organization_id, value, search_value, removed, used_at, removed_at)
            VALUES ('11111111-1111-4111-8111-111111111111', %s, %s, %s,
                NOW() - %s * INTERVAL '1 day',
                CASE WHEN %s THEN NOW() - %s * INTERVAL '1 day' END)
        ''',
            (value, value, removed, 400 if removed else age_days, removed, age_days),
        )
    await service_client.run_periodic_task('cargo-suggestions-cleanup')
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT COUNT(*) FROM cargo_suggestions WHERE value = %s', (value,))
        assert cursor.fetchone()[0] == int(survives)


@pytest.mark.config(
    CARGO_SUGGESTIONS_RETENTION={
        'enabled': True,
        'active_days': 2,
        'removed_days': 1,
        'batch_size': 2,
    }
)
async def test_custom_retention_drains_multiple_batches(service_client, pgsql):
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('''
            INSERT INTO cargo_suggestions (organization_id, value, search_value, used_at)
            SELECT '11111111-1111-4111-8111-111111111111', 'Просрочено ' || n, 'просрочено ' || n,
                NOW() - INTERVAL '3 days'
            FROM generate_series(1, 5) AS n
        ''')
    await service_client.run_periodic_task('cargo-suggestions-cleanup')
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT COUNT(*) FROM cargo_suggestions')
        assert cursor.fetchone()[0] == 0


async def test_removal_time_and_old_shipment_updates(api, pgsql, service_client):
    description = f'Ошибочное описание {uuid4()}'
    created = (
        await api.post(
            '/api/v1/shipments/create',
            {
                'customer_id': str(uuid4()),
                'idempotency_key': str(uuid4()),
                'cargo_description': description,
            },
            expected=201,
        )
    )['shipment']
    await api.post('/api/v1/shipments/cargo-suggestions/remove', {'value': description})
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute(
            'SELECT removed_at IS NOT NULL FROM cargo_suggestions WHERE value = %s', (description,)
        )
        assert cursor.fetchone()[0]
        cursor.execute("UPDATE cargo_suggestions SET removed_at = NOW() - INTERVAL '31 days'")
    await service_client.run_periodic_task('cargo-suggestions-cleanup')
    await api.post(
        '/api/v1/shipments/update',
        {
            'shipment_id': created['id'],
            'expected_revision': 1,
            'reference': 'Уточнение',
        },
    )
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT COUNT(*) FROM cargo_suggestions WHERE value = %s', (description,))
        assert cursor.fetchone()[0] == 0
    stored = (await api.post('/api/v1/shipments/get', {'shipment_id': created['id']}))['shipment']
    assert stored['cargo_description'] == description


@pytest.mark.config(
    CARGO_SUGGESTIONS_RETENTION={
        'enabled': False,
        'active_days': 1,
        'removed_days': 1,
        'batch_size': 1000,
    }
)
async def test_cleanup_can_be_disabled(service_client, pgsql):
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute("""
            INSERT INTO cargo_suggestions (organization_id, value, search_value, used_at)
            VALUES ('11111111-1111-4111-8111-111111111111', 'Сохранить', 'сохранить',
                NOW() - INTERVAL '400 days')
        """)
    await service_client.run_periodic_task('cargo-suggestions-cleanup')
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT COUNT(*) FROM cargo_suggestions')
        assert cursor.fetchone()[0] == 1


@pytest.mark.config(
    CARGO_SUGGESTIONS_RETENTION={
        'enabled': True,
        'active_days': 2,
        'removed_days': 1,
        'batch_size': 2,
    }
)
async def test_backlog_continues_in_bounded_passes(service_client, pgsql):
    await service_client.run_periodic_task('cargo-suggestions-cleanup')
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('''
            INSERT INTO cargo_suggestions (organization_id, value, search_value, used_at)
            SELECT '11111111-1111-4111-8111-111111111111', 'Очередь ' || n, 'очередь ' || n,
                NOW() - INTERVAL '3 days'
            FROM generate_series(1, 25) AS n
        ''')
    await service_client.run_periodic_task('cargo-suggestions-cleanup')
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT COUNT(*) FROM cargo_suggestions')
        assert cursor.fetchone()[0] == 5
    await service_client.run_periodic_task('cargo-suggestions-cleanup-backlog')
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT COUNT(*) FROM cargo_suggestions')
        assert cursor.fetchone()[0] == 0
