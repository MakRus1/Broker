from pathlib import Path

import pytest
import yaml
from pytest_userver.client import TestsuiteTaskFailed as TaskFailed
from test_airport_sync import feed

KEY = 'AIRPORT_SYNC_SETTINGS'


def policy(**changes):
    path = Path(__file__).resolve().parents[3] / 'schemas/configs/transport_locations' / (KEY + '.yaml')
    return {**yaml.safe_load(path.read_text())['default'], 'http_timeout_ms': 1000, **changes}


async def apply(service_client, dynamic_config, **changes):
    dynamic_config.set(**{KEY: policy(**changes)})
    await service_client.update_server_state()


async def test_pause_and_resume_without_restart(service_client, dynamic_config, feed, pgsql):
    await apply(service_client, dynamic_config, enabled=False)
    await service_client.run_periodic('airport-sync')
    assert feed['calls'] == 0
    cursor = pgsql['db_1'].cursor()
    cursor.execute("SELECT failure_count, lease FROM directory_sync WHERE name = 'ourairports'")
    assert cursor.fetchone() == (0, None)
    await apply(service_client, dynamic_config, enabled=True)
    await service_client.run_periodic('airport-sync')
    assert feed['calls'] == 1


async def test_period_changes_apply_to_already_scheduled_source(service_client, dynamic_config, feed, pgsql):
    await service_client.run_periodic('airport-sync')
    cursor = pgsql['db_1'].cursor()
    cursor.execute("UPDATE directory_sync SET updated_at = now() - INTERVAL '2 hours' WHERE name = 'ourairports'")
    await service_client.run_periodic('airport-sync')
    assert feed['calls'] == 1
    await apply(service_client, dynamic_config, period_seconds=3600)
    await service_client.run_periodic('airport-sync')
    assert feed['calls'] == 2
    cursor.execute("UPDATE directory_sync SET updated_at = now() - INTERVAL '2 days' WHERE name = 'ourairports'")
    await apply(service_client, dynamic_config, period_seconds=259200)
    await service_client.run_periodic('airport-sync')
    assert feed['calls'] == 2


async def test_retry_backoff_cap_and_live_changes(service_client, dynamic_config, feed, pgsql):
    await apply(service_client, dynamic_config, retry_initial_seconds=10, retry_max_seconds=15)
    feed['status'] = 503
    cursor = pgsql['db_1'].cursor()
    for attempt, delay in [(1, 10), (2, 15)]:
        with pytest.raises(TaskFailed):
            await service_client.run_periodic('airport-sync')
        cursor.execute("SELECT failure_count, extract(epoch FROM next_attempt_at - last_attempt_at)::int, lease FROM directory_sync WHERE name = 'ourairports'")
        assert cursor.fetchone() == (attempt, delay, None)
        calls = feed['calls']
        await service_client.run_periodic('airport-sync')
        assert feed['calls'] == calls
        cursor.execute("UPDATE directory_sync SET last_attempt_at = now() - INTERVAL '11 seconds' WHERE name = 'ourairports'")
    feed['status'] = 200
    await apply(service_client, dynamic_config, retry_initial_seconds=1, retry_max_seconds=2)
    await service_client.run_periodic('airport-sync')
    cursor.execute("SELECT failure_count, record_count, lease FROM directory_sync WHERE name = 'ourairports'")
    assert cursor.fetchone() == (0, 1, None)


async def test_active_lease_is_not_taken_over_after_policy_change(service_client, dynamic_config, feed, pgsql):
    cursor = pgsql['db_1'].cursor()
    cursor.execute("UPDATE directory_sync SET lease = '00000000-0000-0000-0000-000000000001', lease_until = now() + INTERVAL '1 minute' WHERE name = 'ourairports'")
    await apply(service_client, dynamic_config, period_seconds=60)
    await service_client.run_periodic('airport-sync')
    assert feed['calls'] == 0
    cursor.execute("UPDATE directory_sync SET lease_until = now() - INTERVAL '1 second' WHERE name = 'ourairports'")
    await service_client.run_periodic('airport-sync')
    assert feed['calls'] == 1


async def test_http_retries_change_without_restart(service_client, dynamic_config, feed, pgsql):
    from test_airport_sync import due
    feed['status'] = 503
    await apply(service_client, dynamic_config, http_retries=1)
    with pytest.raises(TaskFailed):
        await service_client.run_periodic('airport-sync')
    assert feed['calls'] == 2
    due(pgsql)
    await apply(service_client, dynamic_config, http_retries=0)
    with pytest.raises(TaskFailed):
        await service_client.run_periodic('airport-sync')
    assert feed['calls'] == 3


async def test_http_timeout_changes_without_restart(service_client, dynamic_config, feed, pgsql, mockserver):
    import asyncio
    from test_airport_sync import due

    @mockserver.handler('/airports.csv')
    async def slow_feed(request):
        await asyncio.sleep(0.25)
        return mockserver.make_response(feed['body'])

    await apply(service_client, dynamic_config, http_timeout_ms=100)
    with pytest.raises(TaskFailed):
        await service_client.run_periodic('airport-sync')
    due(pgsql)
    await apply(service_client, dynamic_config, http_timeout_ms=1000)
    await service_client.run_periodic('airport-sync')
    cursor = pgsql['db_1'].cursor()
    cursor.execute('SELECT count(*) FROM route_locations')
    assert cursor.fetchone()[0] == 1
