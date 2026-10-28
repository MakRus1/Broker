import pytest
from pytest_userver.client import TestsuiteTaskFailed as TaskFailed
from test_airport_sync import feed, due


@pytest.fixture
def slow_claim(pgsql):
    cursor = pgsql['db_1'].cursor()
    cursor.execute("""
        CREATE FUNCTION slow_directory_claim() RETURNS trigger LANGUAGE plpgsql AS $$
        BEGIN
            IF NEW.lease IS NOT NULL THEN PERFORM pg_sleep(0.2); END IF;
            RETURN NEW;
        END;
        $$;
        CREATE TRIGGER slow_directory_claim BEFORE UPDATE ON directory_sync
            FOR EACH ROW EXECUTE FUNCTION slow_directory_claim();
    """)
    yield
    cursor.execute('DROP TRIGGER slow_directory_claim ON directory_sync; DROP FUNCTION slow_directory_claim();')


# Общий тайм-аут влияет и на создание фоновых соединений пула.
# Не переносим их состояние между независимыми вариантами теста.
@pytest.mark.uservice_oneshot
@pytest.mark.parametrize('key,value', [
    ('POSTGRES_DEFAULT_COMMAND_CONTROL', {'network_timeout_ms': 1000, 'statement_timeout_ms': 50}),
    ('POSTGRES_QUERIES_COMMAND_CONTROL', {'claim_directory_sync': {'network_timeout_ms': 1000, 'statement_timeout_ms': 50}}),
])
async def test_live_default_and_query_timeouts(service_client, dynamic_config, feed, slow_claim, key, value):
    dynamic_config.set(**{key: value})
    await service_client.update_server_state()
    with pytest.raises(TaskFailed):
        await service_client.run_periodic('airport-sync')
    assert feed['calls'] == 0
    high = {'network_timeout_ms': 2000, 'statement_timeout_ms': 1000}
    dynamic_config.set(**{key: high if key == 'POSTGRES_DEFAULT_COMMAND_CONTROL' else {'claim_directory_sync': high}})
    await service_client.update_server_state()
    await service_client.run_periodic('airport-sync')
    assert feed['calls'] == 1


async def test_live_named_transaction_timeout_covers_commit(service_client, dynamic_config, feed, pgsql):
    cursor = pgsql['db_1'].cursor()
    cursor.execute("""
        CREATE FUNCTION slow_airport_commit() RETURNS trigger LANGUAGE plpgsql AS $$
        BEGIN
            PERFORM pg_sleep(0.2);
            RETURN NEW;
        END;
        $$;
        CREATE CONSTRAINT TRIGGER slow_airport_commit AFTER INSERT ON route_locations
            DEFERRABLE INITIALLY DEFERRED FOR EACH ROW EXECUTE FUNCTION slow_airport_commit();
    """)
    try:
        dynamic_config.set(POSTGRES_QUERIES_COMMAND_CONTROL={
            'directory-sync.airports': {'network_timeout_ms': 100, 'statement_timeout_ms': 50}})
        await service_client.update_server_state()
        with pytest.raises(TaskFailed):
            await service_client.run_periodic('airport-sync')
        cursor.execute('SELECT count(*) FROM route_locations')
        assert cursor.fetchone()[0] == 0
        due(pgsql)
        dynamic_config.set(POSTGRES_QUERIES_COMMAND_CONTROL={
            'directory-sync.airports': {'network_timeout_ms': 2000, 'statement_timeout_ms': 1000}})
        await service_client.update_server_state()
        await service_client.run_periodic('airport-sync')
        cursor.execute('SELECT count(*) FROM route_locations')
        assert cursor.fetchone()[0] == 1
    finally:
        cursor.execute('DROP TRIGGER slow_airport_commit ON route_locations; DROP FUNCTION slow_airport_commit();')


async def test_live_handler_sql_timeout(service_client, dynamic_config, pgsql, auth_key):
    cursor = pgsql['db_1'].cursor()
    cursor.execute("""
        CREATE FUNCTION slow_manual_station() RETURNS trigger LANGUAGE plpgsql AS $$
        BEGIN
            PERFORM pg_sleep(0.2);
            RETURN NEW;
        END;
        $$;
        CREATE TRIGGER slow_manual_station BEFORE INSERT ON railway_stations
            FOR EACH ROW EXECUTE FUNCTION slow_manual_station();
    """)
    path = '/api/admin/railway-stations/save'
    token = auth_key.token('author', roles=['platform.admin'], claims={
        'aud': 'broker-logistics', 'resource_access': {'broker-logistics': {'roles': ['platform.admin']}}})
    body = {'id': '00000000-0000-0000-0000-000000000123', 'name': 'Тестовая', 'country_code': 'RU'}
    try:
        dynamic_config.set(POSTGRES_HANDLERS_COMMAND_CONTROL={path: {
            'POST': {'network_timeout_ms': 1000, 'statement_timeout_ms': 50}}})
        await service_client.update_server_state()
        response = await service_client.post(path, json=body, headers={'Authorization': 'Bearer ' + token})
        assert response.status == 500
        dynamic_config.set(POSTGRES_HANDLERS_COMMAND_CONTROL={path: {
            'POST': {'network_timeout_ms': 2000, 'statement_timeout_ms': 1000}}})
        await service_client.update_server_state()
        response = await service_client.post(path, json=body, headers={'Authorization': 'Bearer ' + token})
        assert response.status == 200, response.text
    finally:
        cursor.execute('DROP TRIGGER slow_manual_station ON railway_stations; DROP FUNCTION slow_manual_station();')
