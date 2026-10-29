"""Общий SQL outbox: lease, устаревшие подтверждения и замена последнего состояния."""

import json
import re
from uuid import uuid4

import pytest


@pytest.fixture
def outbox_sql(pgsql, service_source_dir):
    directory = service_source_dir.parents[1] / "libs/outbox/postgresql/queries"

    def execute(name, *parameters):
        source = (directory / f"{name}.sql").read_text()
        values = []

        def bind(match):
            values.append(parameters[int(match[1]) - 1])
            return "%s"

        statement = re.sub(r"\$(\d+)", bind, source)
        with pgsql["db_1"].cursor() as cursor:
            cursor.execute(statement, values)
            return cursor.fetchall() if cursor.description else cursor.rowcount

    return execute


def enqueue(sql, topic="test", key=None, payload=None, latest=False):
    key = key or str(uuid4())
    sql(
        "enqueue_latest" if latest else "enqueue",
        topic,
        key,
        json.dumps(payload or {"n": 1}),
    )
    return key


def test_claims_are_exclusive_and_scoped_to_registered_topics(outbox_sql):
    sql = outbox_sql
    enqueue(sql, topic="other")
    enqueue(sql)
    enqueue(sql)
    first = sql("claim", "test", 30)[0]
    second = sql("claim", "test", 30)[0]
    assert first[0] != second[0]
    assert first[1] == second[1] == "test"
    assert sql("claim", "test", 30) == []
    assert len(sql("claim", "other", 30)) == 1


def test_expired_lease_is_reclaimed_and_old_ack_cannot_remove_it(outbox_sql, pgsql):
    sql = outbox_sql
    enqueue(sql)
    old = sql("claim", "test", 30)[0]
    with pgsql["db_1"].cursor() as cursor:
        cursor.execute(
            "UPDATE outbox SET available_at=now()-INTERVAL '1 second' WHERE id=%s",
            (old[0],),
        )
    current = sql("claim", "test", 30)[0]
    assert current[0] == old[0]
    assert current[3] != old[3]
    assert sql("lock_delivery", old[0], old[3], old[4]) == []
    assert sql("complete", old[0], old[3], old[4]) == 0
    assert sql("retry", old[0], old[3], 2, 60) == 0
    assert len(sql("lock_delivery", current[0], current[3], current[4])) == 1
    assert sql("complete", current[0], current[3], current[4]) == 1


def test_latest_state_invalidates_old_delivery(outbox_sql):
    sql = outbox_sql
    key = enqueue(sql)
    old = sql("claim", "test", 30)[0]
    enqueue(sql, key=key, payload={"n": 2}, latest=True)
    assert sql("lock_delivery", old[0], old[3], old[4]) == []
    assert sql("complete", old[0], old[3], old[4]) == 0
    assert sql("retry", old[0], old[3], 2, 60) == 0
    current = sql("claim", "test", 30)[0]
    assert current[0] == old[0]
    assert current[2] == {"n": 2}
    assert current[4] == old[4] + 1


def test_enqueue_is_idempotent_while_pending(outbox_sql):
    sql = outbox_sql
    key = enqueue(sql)
    enqueue(sql, key=key, payload={"n": 2})
    assert sql("claim", "test", 30)[0][2] == {"n": 1}
    assert sql("claim", "test", 30) == []


def test_retry_uses_bounded_delay_and_releases_lease(outbox_sql, pgsql):
    sql = outbox_sql
    enqueue(sql)
    message = sql("claim", "test", 30)[0]
    with pgsql["db_1"].cursor() as cursor:
        cursor.execute("UPDATE outbox SET attempts=100 WHERE id=%s", (message[0],))
    assert sql("retry", message[0], message[3], 2, 60) == 1
    with pgsql["db_1"].cursor() as cursor:
        cursor.execute(
            "SELECT lease_token, available_at > now()+INTERVAL '59 seconds', available_at <= now()+INTERVAL '60 seconds' FROM outbox WHERE id=%s",
            (message[0],),
        )
        assert cursor.fetchone() == (None, True, True)
    assert sql("claim", "test", 30) == []


def test_enqueue_rolls_back_with_business_transaction(outbox_sql, pgsql):
    with pgsql["db_1"].cursor() as cursor:
        cursor.execute("BEGIN")
        enqueue(outbox_sql)
        cursor.execute("ROLLBACK")
    assert outbox_sql("claim", "test", 30) == []


@pytest.fixture(scope="session")
def parallel_outbox(configure_dispatcher):
    def patch(config, config_vars):
        configure_dispatcher(config, config_vars)
        config["components_manager"]["components"]["platform-outbox"].update(
            {
                "max-concurrency": 3,
                "default-topic-concurrency": 1,
                "topic-concurrency": {
                    "transportations.create": 2,
                    "shipment.route.project": 1,
                },
                "idle-pause-ms": 20,
                "busy-pause-ms": 1,
            }
        )

    return patch


@pytest.mark.uservice_oneshot(config_hooks=["parallel_outbox"])
async def test_slow_topic_does_not_block_other_topic_and_limits_hold(
    api, mockserver, monitor_client
):
    import asyncio
    from test_shipments import setup_item

    release = asyncio.Event()
    two_sending = asyncio.Event()
    route_delivered = asyncio.Event()
    active = 0
    maximum = 0
    delivered = 0
    watch_routes = False

    @mockserver.json_handler("/service-token")
    def token(request):
        return {"access_token": "parallel-token"}

    @mockserver.json_handler("/api/internal/v1/transportations/ensure-created")
    async def operation(request):
        nonlocal active, maximum, delivered
        active += 1
        maximum = max(maximum, active)
        if active == 2:
            two_sending.set()
        try:
            await release.wait()
            body = request.json
            delivered += 1
            return {
                "operation": {
                    "id": body["operation_id"],
                    "item_id": body["item_id"],
                    "quantity": body["quantity"],
                    "status": "succeeded",
                    "transportation_ids": [
                        str(uuid4()) for _ in range(body["quantity"])
                    ],
                }
            }
        finally:
            active -= 1

    @mockserver.json_handler("/api/internal/v1/shipments/route-projection")
    def route(request):
        if watch_routes:
            route_delivered.set()
        return {"applied": True}

    try:
        for _ in range(3):
            _, item = await setup_item(api)
            await api.post(
                "/api/v1/transportation-operations/create",
                {
                    "item_id": item["id"],
                    "expected_revision": 1,
                    "quantity": 1,
                    "idempotency_key": str(uuid4()),
                },
                expected=202,
            )
        await asyncio.wait_for(two_sending.wait(), timeout=3)
        # Оба выделенных слота создания заняты. Новый маршрут должен пройти
        # через свой слот до освобождения заблокированного HTTP-обработчика.
        watch_routes = True
        await api.post(
            "/api/v1/shipments/create",
            {
                "customer_id": str(uuid4()),
                "idempotency_key": str(uuid4()),
            },
            expected=201,
        )
        await asyncio.wait_for(route_delivered.wait(), timeout=1)
        assert active == 2
        assert delivered == 0
        assert maximum == 2
        from test_outbox_metrics import metric

        text = (await monitor_client.get("/metrics")).text
        assert metric(text, "outbox_active", "transportations.create") == 2
        assert metric(text, "outbox_concurrency", "transportations.create") == 2
        assert metric(text, "outbox_concurrency", "shipment.route.project") == 1
    finally:
        release.set()
    for _ in range(100):
        if delivered == 3:
            break
        await asyncio.sleep(0.02)
    assert delivered == 3
    assert maximum == 2
