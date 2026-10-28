import csv
import io
import json
from uuid import uuid4

import pytest
from pytest_userver.client import TestsuiteTaskFailed as TaskFailed
from test_airport_sync import feed


def dataset(*rows):
    output = io.StringIO()
    writer = csv.writer(output)
    writer.writerows(rows or [['', 'AQ', '', '.ANTARCTICA', '', '', '', '', '', '', '', '']])
    return output.getvalue()


def port(code='CNSHA', name='Shanghai', *, change='', date='2501', status='AA', function='1-------', alias=None):
    return [change, code[:2], code[2:], name, alias or name, '', function, status, date, '', '', '']


@pytest.fixture
def port_feed(mockserver, pgsql):
    pgsql['db_1'].cursor().execute(
        "INSERT INTO directory_sync(name, next_attempt_at) VALUES ('unlocode', now())"
    )
    state = {'tags': [{'name': '2025-1'}], 'parts': [dataset(port()), dataset(), dataset()],
             'status': 200, 'calls': []}

    @mockserver.json_handler('/unlocode/releases')
    def releases(request):
        state['calls'].append('tags')
        return state['tags']

    def handler(index, release):
        def download(request):
            state['calls'].append((release, index))
            assert request.query['job'] == 'package-release'
            return mockserver.make_response(state['parts'][index], state['status'])
        return download

    for release in ('2025-1', '2026-1'):
        for index in range(3):
            mockserver.handler(
                f'/unlocode/artifacts/{release}/raw/release/csv/UNLOCODE CodeListPart{index + 1}.csv'
            )(handler(index, release))
    return state


def due(pgsql):
    pgsql['db_1'].cursor().execute(
        "UPDATE directory_sync SET next_attempt_at = now() - INTERVAL '1 second' WHERE name = 'unlocode'"
    )


def locations(pgsql):
    cursor = pgsql['db_1'].cursor()
    cursor.execute("SELECT id::text, name, code, archived FROM route_locations WHERE kind = 'sea_port' ORDER BY code")
    return cursor.fetchall()


async def test_import_selects_latest_official_release_and_updates_same_uuid(service_client, port_feed, pgsql):
    port_feed['tags'] = [{'name': '2025-1'}, {'name': 'latest'}, {'name': '2026-1-rc'}, {'name': '2026-1'}]
    await service_client.run_periodic('sea-port-sync')
    original = locations(pgsql)
    assert len(original) == 1 and original[0][1:] == ('Shanghai', 'CNSHA', False)
    assert port_feed['calls'] == ['tags', ('2026-1', 0), ('2026-1', 1), ('2026-1', 2)]
    port_feed['parts'][0] = dataset(port(name='New Shanghai'))
    due(pgsql)
    await service_client.run_periodic('sea-port-sync')
    assert locations(pgsql) == [(original[0][0], 'New Shanghai', 'CNSHA', False)]
    cursor = pgsql['db_1'].cursor()
    cursor.execute("SELECT source_revision, record_count, length(source_hash) FROM directory_sync WHERE name = 'unlocode'")
    assert cursor.fetchone() == ('2026-1', 1, 64)


async def test_only_confirmed_seaports_are_imported(service_client, port_feed, pgsql):
    port_feed['parts'][0] = dataset(port(), port('CNAAA', change='X'), port('CNAAB', status='RQ'),
        port('CNAAC', status='QQ'), port('CNAAD', function='-------8'), port('CNAAE', function='--3-----'))
    await service_client.run_periodic('sea-port-sync')
    assert [row[2] for row in locations(pgsql)] == ['CNSHA']


async def test_aliases_and_diacritics_are_searchable(service_client, port_feed, pgsql, api):
    port_feed['parts'][0] = dataset(port(name='Older Shanghai', date='9601'),
        port(name='Shànghǎi', change='#', alias='Shanghai', date='2501'))
    await service_client.run_periodic('sea-port-sync')
    assert len(locations(pgsql)) == 1
    assert locations(pgsql)[0][1] == 'Shànghǎi'
    for query in ('Shanghai', 'Older Shanghai', 'CNSHA', 'Shànghǎi'):
        result = await api.post('/api/v1/route-locations/search', {'kind': 'sea_port', 'query': query})
        assert len(result['items']) == 1
    port_feed['parts'][0] = dataset(port(name='Shànghǎi', change='#', alias='Shanghai', date='2501'),
        port(name='Older Shanghai', date='9601'))
    previous = locations(pgsql)
    due(pgsql)
    await service_client.run_periodic('sea-port-sync')
    assert locations(pgsql) == previous


@pytest.mark.parametrize('code', ['CNSHA', 'SHA'])
async def test_existing_manual_port_is_linked_and_archive_preserved(service_client, port_feed, pgsql, api, code):
    item = {'id': str(uuid4()), 'kind': 'sea_port', 'country_code': 'CN', 'code': code, 'name': 'Шанхай'}
    await api.post('/api/admin/route-locations/save', item, roles=['platform.admin'])
    await service_client.run_periodic('sea-port-sync')
    assert locations(pgsql) == [(item['id'], 'Шанхай', code, False)]
    found = await api.post('/api/v1/route-locations/search', {'kind': 'sea_port', 'query': 'CNSHA'})
    assert found['items'][0]['id'] == item['id']
    await api.post('/api/admin/route-locations/archive', {'id': item['id']}, roles=['platform.admin'])
    port_feed['parts'][0] = dataset(port(name='Changed by source'))
    due(pgsql)
    await service_client.run_periodic('sea-port-sync')
    assert locations(pgsql) == [(item['id'], 'Шанхай', code, True)]


async def test_edit_of_imported_port_is_preserved(service_client, port_feed, pgsql, api):
    await service_client.run_periodic('sea-port-sync')
    item = (await api.post('/api/v1/route-locations/search', {'kind': 'sea_port'}))['items'][0]
    item.update(name='Шанхай', existing=True)
    await api.post('/api/admin/route-locations/save', item, roles=['platform.admin'])
    due(pgsql)
    await service_client.run_periodic('sea-port-sync')
    assert locations(pgsql)[0][1] == 'Шанхай'


@pytest.mark.parametrize('body', ['<html>Bad gateway</html>', '', dataset(port('CN1AB')), 'broken,"quote'])
async def test_invalid_part_leaves_previous_snapshot(service_client, port_feed, pgsql, body):
    await service_client.run_periodic('sea-port-sync')
    previous = locations(pgsql)
    port_feed['parts'][2] = body
    due(pgsql)
    with pytest.raises(TaskFailed):
        await service_client.run_periodic('sea-port-sync')
    assert locations(pgsql) == previous
    cursor = pgsql['db_1'].cursor()
    cursor.execute("SELECT record_count FROM directory_sync WHERE name = 'unlocode'")
    assert cursor.fetchone() == (1,)


@pytest.mark.parametrize('tags', [[], [{'name': 'latest'}], [{'name': '2026-1-rc'}], {'error': 'no data'}])
async def test_invalid_release_list_is_rejected(service_client, port_feed, pgsql, tags):
    port_feed['tags'] = tags
    with pytest.raises(TaskFailed):
        await service_client.run_periodic('sea-port-sync')
    assert locations(pgsql) == []
    assert port_feed['calls'] == ['tags']


async def test_lease_prevents_duplicate_download_and_sources_are_independent(service_client, port_feed, feed, pgsql):
    await service_client.run_periodic('sea-port-sync')
    calls = port_feed['calls'].copy()
    await service_client.run_periodic('sea-port-sync')
    assert port_feed['calls'] == calls
    await service_client.run_periodic('airport-sync')
    cursor = pgsql['db_1'].cursor()
    cursor.execute('SELECT name, record_count FROM directory_sync ORDER BY name')
    assert cursor.fetchall() == [('ourairports', 1), ('unlocode', 1)]


async def test_http_failure_leaves_previous_snapshot(service_client, port_feed, pgsql):
    await service_client.run_periodic('sea-port-sync')
    previous = locations(pgsql)
    port_feed['status'] = 503
    due(pgsql)
    with pytest.raises(TaskFailed):
        await service_client.run_periodic('sea-port-sync')
    assert locations(pgsql) == previous


async def test_large_drop_is_rejected_and_small_drop_does_not_delete_ports(service_client, port_feed, pgsql):
    port_feed['parts'][0] = dataset(*[port('CNAA' + letter) for letter in 'ABCDE'])
    await service_client.run_periodic('sea-port-sync')
    previous = locations(pgsql)
    port_feed['parts'][0] = dataset(*[port('CNAA' + letter) for letter in 'ABCD'])
    due(pgsql)
    await service_client.run_periodic('sea-port-sync')
    assert locations(pgsql) == previous
    port_feed['parts'][0] = dataset(port('CNAAB'))
    due(pgsql)
    with pytest.raises(TaskFailed):
        await service_client.run_periodic('sea-port-sync')
    assert locations(pgsql) == previous


async def test_ambiguous_manual_codes_roll_back_whole_publication(service_client, port_feed, pgsql, api):
    await service_client.run_periodic('sea-port-sync')
    previous = locations(pgsql)
    for code in ('CNABC', 'ABC'):
        await api.post('/api/admin/route-locations/save', {
            'id': str(uuid4()), 'kind': 'sea_port', 'name': 'Ручной порт', 'country_code': 'CN', 'code': code
        }, roles=['platform.admin'])
    port_feed['parts'][0] = dataset(port(name='New name'), port('CNABC'))
    due(pgsql)
    with pytest.raises(TaskFailed):
        await service_client.run_periodic('sea-port-sync')
    assert [row for row in locations(pgsql) if row[2] == 'CNSHA'] == previous
    cursor = pgsql['db_1'].cursor()
    cursor.execute('SELECT locode, name FROM sea_port_sources')
    assert cursor.fetchall() == [('CNSHA', 'Shanghai')]
