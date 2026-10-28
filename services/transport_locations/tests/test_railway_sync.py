from uuid import UUID

import pytest
from pytest_userver.client import TestsuiteTaskFailed as TaskFailed


def station(source=1, name='Алматы', country='KZ', **codes):
    values = dict(station=f'https://www.openstreetmap.org/node/{source}',
                  name=name, country_code=country, kind='station', **codes)
    return {key: {'type': 'uri' if key == 'station' else 'literal', 'value': value}
            for key, value in values.items()}


@pytest.fixture
def railway_feed(mockserver, pgsql):
    pgsql['db_1'].cursor().execute(
        "INSERT INTO directory_sync(name, next_attempt_at) VALUES ('osm-railway', now())")
    state = {'rows': [station(esr_hint='700101', uic='2700101')], 'status': 200, 'calls': 0}

    @mockserver.json_handler('/railway')
    def feed(request):
        state['calls'] += 1
        assert 'SELECT DISTINCT' in request.query['query']
        document = {'results': {'bindings': state['rows']}}
        if 'total' in state:
            document['meta'] = {'result-size-total': state['total']}
        return mockserver.make_response(json=document, status=state['status'])
    return state


def due(pgsql):
    pgsql['db_1'].cursor().execute(
        "UPDATE directory_sync SET next_attempt_at = now() - INTERVAL '1 second' WHERE name = 'osm-railway'")


async def search(api, query=''):
    return (await api.post('/api/v1/railway-stations/search', {'query': query}))['items']


async def test_import_identity_optional_codes_and_weekly_schedule(service_client, railway_feed, pgsql, api):
    await service_client.run_periodic('railway-sync')
    first, = await search(api, '700101')
    UUID(first['id'])
    assert first['esr_source'] == 'osm_hint'
    assert first['uic_code'] == '2700101'
    assert (await search(api, '2700101')) == [first]
    await service_client.run_periodic('railway-sync')
    assert railway_feed['calls'] == 1
    cursor = pgsql['db_1'].cursor()
    cursor.execute("SELECT next_attempt_at - updated_at FROM directory_sync WHERE name = 'osm-railway'")
    assert cursor.fetchone()[0].days == 7
    railway_feed['rows'] = [station(name='Almaty', name_ru='Алматы')]
    due(pgsql)
    await service_client.run_periodic('railway-sync')
    updated, = await search(api, 'Алматы')
    assert updated['id'] == first['id']
    assert updated['name'] == 'Almaty'
    assert 'esr_code' not in updated
    assert 'esr_source' not in updated


async def test_manual_edits_and_archive_survive_refresh(service_client, railway_feed, pgsql, api):
    await service_client.run_periodic('railway-sync')
    first, = await search(api)
    body = {key: first[key] for key in ('id', 'name', 'country_code', 'esr_code', 'uic_code')}
    body.update(name='Ручное название', existing=True)
    saved = await api.post('/api/admin/railway-stations/save', body, roles=['platform.admin'])
    assert saved['esr_source'] == 'manual'
    due(pgsql)
    await service_client.run_periodic('railway-sync')
    assert await search(api) == [saved]
    await api.post('/api/admin/railway-stations/archive', {'id': first['id']}, roles=['platform.admin'])
    due(pgsql)
    await service_client.run_periodic('railway-sync')
    assert await search(api) == []


async def test_ambiguous_country_is_skipped_without_merging_names_or_codes(service_client, railway_feed, api):
    railway_feed['rows'] = [station(1), station(1, country='CN'),
                            station(2, esr_hint='700101'), station(3, esr_hint='700101')]
    await service_client.run_periodic('railway-sync')
    rows = await search(api)
    assert len(rows) == 2
    assert len({row['id'] for row in rows}) == 2


@pytest.mark.parametrize('code', ['0', '000000', '12345', '1234567', 'ABCDEF', '123456;654321'])
async def test_invalid_esr_hint_does_not_prevent_station_import(service_client, railway_feed, api, code):
    railway_feed['rows'] = [station(esr_hint=code)]
    await service_client.run_periodic('railway-sync')
    row, = await search(api)
    assert 'esr_code' not in row


@pytest.mark.parametrize('problem', ['empty', 'bad_source', 'http', 'large_drop', 'truncated'])
async def test_invalid_feed_preserves_published_directory(service_client, railway_feed, pgsql, api, problem):
    await service_client.run_periodic('railway-sync')
    before = await search(api)
    if problem == 'empty':
        railway_feed['rows'] = []
    elif problem == 'bad_source':
        railway_feed['rows'][0]['station']['value'] = 'https://example.org/node/1'
    elif problem == 'http':
        railway_feed['status'] = 503
    elif problem == 'truncated':
        railway_feed['total'] = 2
    else:
        pgsql['db_1'].cursor().execute("UPDATE directory_sync SET record_count = 100 WHERE name = 'osm-railway'")
    due(pgsql)
    with pytest.raises(TaskFailed):
        await service_client.run_periodic('railway-sync')
    assert await search(api) == before
