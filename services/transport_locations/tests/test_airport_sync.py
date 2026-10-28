import csv
import io
from uuid import uuid4

import pytest
from pytest_userver.client import TestsuiteTaskFailed as TaskFailed


FIELDS = ['id', 'type', 'name', 'iso_country', 'iata_code', 'icao_code', 'municipality', 'keywords']


def dataset(*rows):
    output = io.StringIO()
    writer = csv.writer(output)
    writer.writerow(FIELDS)
    writer.writerows(rows)
    return output.getvalue()


def airport(source=1, name='Heathrow', iata='LHR', icao='EGLL'):
    return [source, 'large_airport', name, 'GB', iata, icao, 'London', 'Лондон']


@pytest.fixture
def feed(mockserver, pgsql):
    # Testsuite очищает таблицы между тестами, включая начальную строку миграции.
    pgsql['db_1'].cursor().execute(
        "INSERT INTO directory_sync(name, next_attempt_at) VALUES ('ourairports', now())"
    )
    state = {'body': dataset(airport()), 'status': 200, 'calls': 0}

    @mockserver.handler('/airports.csv')
    def airports(request):
        state['calls'] += 1
        return mockserver.make_response(state['body'], state['status'])

    return state


def due(pgsql):
    pgsql['db_1'].cursor().execute("UPDATE directory_sync SET next_attempt_at = now() - INTERVAL '1 second', updated_at = now() - INTERVAL '40 days', last_attempt_at = now() - INTERVAL '40 days', lease_until = now() - INTERVAL '1 second' WHERE name = 'ourairports'")


def locations(pgsql):
    cursor = pgsql['db_1'].cursor()
    cursor.execute('SELECT id::text, name, code, archived FROM route_locations ORDER BY id')
    return cursor.fetchall()


async def test_import_updates_same_uuid_and_searches_both_codes(service_client, feed, pgsql, api):
    feed['body'] = dataset(airport(name='Airport, "London"\nHeathrow'),
                           [2, 'closed_airport', 'Closed', 'GB', 'OLD', '', '', ''],
                           [3, 'small_airport', 'No code', 'GB', '', '', '', ''])
    await service_client.run_periodic('airport-sync')
    original = locations(pgsql)
    assert len(original) == 1
    assert original[0][1] == 'Airport, "London"\nHeathrow'
    for query in ('LHR', 'EGLL', 'London', 'Лондон'):
        result = await api.post('/api/v1/route-locations/search', {'kind': 'airport', 'query': query})
        assert result['items'][0]['id'] == original[0][0]
    feed['body'] = dataset(airport(name='New name', iata='NEW'))
    due(pgsql)
    await service_client.run_periodic('airport-sync')
    assert locations(pgsql) == [(original[0][0], 'New name', 'NEW', False)]
    cursor = pgsql['db_1'].cursor()
    cursor.execute('SELECT source_hash, record_count, updated_at IS NOT NULL FROM directory_sync')
    digest, count, updated = cursor.fetchone()
    assert len(digest) == 64 and count == 1 and updated


async def test_manual_changes_and_archive_survive_sync(service_client, feed, api, pgsql):
    existing = {'id': str(uuid4()), 'kind': 'airport', 'name': 'Лондон', 'country_code': 'GB', 'code': 'LHR'}
    await api.post('/api/admin/route-locations/save', existing, roles=['platform.admin'])
    await service_client.run_periodic('airport-sync')
    assert locations(pgsql) == [(existing['id'], 'Лондон', 'LHR', False)]
    await api.post('/api/admin/route-locations/archive', {'id': existing['id']}, roles=['platform.admin'])
    due(pgsql)
    feed['body'] = dataset(airport(name='Changed upstream'))
    await service_client.run_periodic('airport-sync')
    assert locations(pgsql) == [(existing['id'], 'Лондон', 'LHR', True)]


@pytest.mark.parametrize('body', [
    '<html>Bad gateway</html>',
    dataset(),
    dataset(airport(), airport()),
    dataset(airport(), airport(source=2)),
    dataset(airport(iata='bad')),
    ','.join(FIELDS) + '\n1,large_airport,"unfinished',
])
async def test_invalid_snapshot_keeps_previous_data(service_client, feed, pgsql, body):
    await service_client.run_periodic('airport-sync')
    previous = locations(pgsql)
    feed['body'] = body
    due(pgsql)
    with pytest.raises(TaskFailed):
        await service_client.run_periodic('airport-sync')
    assert locations(pgsql) == previous
    cursor = pgsql['db_1'].cursor()
    cursor.execute('SELECT record_count FROM directory_sync')
    assert cursor.fetchone() == (1,)


async def test_failed_download_keeps_data_and_respects_lease(service_client, feed, pgsql):
    await service_client.run_periodic('airport-sync')
    previous = locations(pgsql)
    due(pgsql)
    feed['status'] = 503
    with pytest.raises(TaskFailed):
        await service_client.run_periodic('airport-sync')
    # Повторный запуск не обращается к неисправному источнику до истечения аренды.
    await service_client.run_periodic('airport-sync')
    assert locations(pgsql) == previous


async def test_missing_airports_are_not_deleted(service_client, feed, pgsql):
    feed['body'] = dataset(airport(), airport(source=2, iata='ABC', icao='ABCD'))
    await service_client.run_periodic('airport-sync')
    previous = locations(pgsql)
    feed['body'] = dataset(airport())
    due(pgsql)
    # Подозрительное сокращение более 20% отвергается целиком.
    with pytest.raises(TaskFailed):
        await service_client.run_periodic('airport-sync')
    assert locations(pgsql) == previous


async def test_publish_conflict_rolls_back_source_and_projection(service_client, feed, pgsql, api):
    await service_client.run_periodic('airport-sync')
    await api.post('/api/admin/route-locations/save', {
        'id': str(uuid4()), 'kind': 'airport', 'name': 'Other', 'country_code': 'GB', 'code': 'NEW'
    }, roles=['platform.admin'])
    previous = locations(pgsql)
    feed['body'] = dataset(airport(iata='NEW'))
    due(pgsql)
    with pytest.raises(TaskFailed):
        await service_client.run_periodic('airport-sync')
    assert locations(pgsql) == previous
    cursor = pgsql['db_1'].cursor()
    cursor.execute('SELECT iata_code FROM airport_sources')
    assert cursor.fetchone() == ('LHR',)


async def test_successful_sync_is_not_repeated_before_tomorrow(service_client, feed, pgsql):
    await service_client.run_periodic('airport-sync')
    calls = feed['calls']
    await service_client.run_periodic('airport-sync')
    assert feed['calls'] == calls
    cursor = pgsql['db_1'].cursor()
    cursor.execute("SELECT next_attempt_at > now() + INTERVAL '23 hours', lease FROM directory_sync")
    assert cursor.fetchone() == (True, None)


async def test_edit_of_imported_airport_survives_sync(service_client, feed, pgsql, api):
    await service_client.run_periodic('airport-sync')
    item = (await api.post('/api/v1/route-locations/search', {'kind': 'airport'}))['items'][0]
    item.update(name='Моё название', existing=True)
    await api.post('/api/admin/route-locations/save', item, roles=['platform.admin'])
    feed['body'] = dataset(airport(name='Upstream change', iata='NEW'))
    due(pgsql)
    await service_client.run_periodic('airport-sync')
    assert locations(pgsql)[0][1:3] == ('Моё название', 'LHR')


async def test_existing_icao_record_is_linked_without_duplicate(service_client, feed, pgsql, api):
    item = {'id': str(uuid4()), 'kind': 'airport', 'name': 'Лондон', 'country_code': 'GB', 'code': 'EGLL'}
    await api.post('/api/admin/route-locations/save', item, roles=['platform.admin'])
    await service_client.run_periodic('airport-sync')
    assert locations(pgsql) == [(item['id'], 'Лондон', 'EGLL', False)]


async def test_moderate_snapshot_reduction_preserves_missing_records(service_client, feed, pgsql):
    feed['body'] = dataset(*[airport(source=i, iata='', icao=f'ABC{chr(65+i)}') for i in range(1, 6)])
    await service_client.run_periodic('airport-sync')
    previous = locations(pgsql)
    feed['body'] = dataset(*[airport(source=i, iata='', icao=f'ABC{chr(65+i)}') for i in range(1, 5)])
    due(pgsql)
    await service_client.run_periodic('airport-sync')
    assert locations(pgsql) == previous
