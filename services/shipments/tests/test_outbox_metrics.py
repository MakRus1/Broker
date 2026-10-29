"""Prometheus outbox: снимок очереди, свежесть и исходы доставки."""

import asyncio
import re
from uuid import uuid4

import pytest


def metric(text, name, topic=None):
    values = []
    for line in text.splitlines():
        match = re.fullmatch(
            r"([a-zA-Z_:][a-zA-Z0-9_:]*)(\{.*\})?\s+([-+0-9.eE]+)", line
        )
        if not match or match[1] != name:
            continue
        labels = match[2] or ""
        if topic is not None and f'outbox_topic="{topic}"' not in labels:
            continue
        values.append(float(match[3]))
    assert len(values) == 1, (name, topic, values, text)
    return values[0]


@pytest.mark.uservice_oneshot(config_hooks=["configure_dispatcher"])
async def test_queue_snapshot_age_and_empty_queue(
    api, service_client, monitor_client, pgsql
):
    topic = "shipment.route.project"
    # Отложенные сообщения не забираются worker; время lease не подменяет enqueue age.
    with pgsql["db_1"].cursor() as cursor:
        cursor.execute(
            """
            INSERT INTO outbox (topic, message_key, payload, created_at, available_at)
            VALUES (%s, %s, '{}'::JSONB, now()-INTERVAL '2 minutes', now()+INTERVAL '1 hour'),
                   (%s, %s, '{}'::JSONB, now()-INTERVAL '1 minute', now()+INTERVAL '1 hour')
        """,
            (topic, str(uuid4()), topic, str(uuid4())),
        )
    # Готовность сервиса/регистрация periodic задачи до ручного запуска.
    await api.post("/api/v1/shipments/list", {})
    await service_client.run_periodic_task("outbox-queue-statistics")
    response = await monitor_client.get("/metrics")
    assert response.status == 200
    text = response.text
    assert metric(text, "outbox_queue_size", topic) == 2
    assert 115 <= metric(text, "outbox_oldest_message_age_seconds", topic) < 180
    assert metric(text, "outbox_active", topic) == 0
    assert metric(text, "outbox_concurrency", topic) == 2
    assert metric(text, "outbox_queue_snapshot_success") == 1
    assert metric(text, "outbox_queue_snapshot_timestamp_seconds") > 0
    timestamp = metric(text, "outbox_queue_snapshot_timestamp_seconds")
    # Ошибка сбора не превращает прежнюю непустую очередь в «здоровый ноль».
    with pgsql["db_1"].cursor() as cursor:
        cursor.execute(
            "ALTER TABLE outbox RENAME COLUMN created_at TO hidden_created_at"
        )
    try:
        with pytest.raises(service_client.PeriodicTaskFailed):
            await service_client.run_periodic_task("outbox-queue-statistics")
        stale = (await monitor_client.get("/metrics")).text
        assert metric(stale, "outbox_queue_snapshot_success") == 0
        assert metric(stale, "outbox_queue_snapshot_errors_total") == 1
        assert metric(stale, "outbox_queue_snapshot_timestamp_seconds") == timestamp
        assert metric(stale, "outbox_queue_size", topic) == 2
    finally:
        with pgsql["db_1"].cursor() as cursor:
            cursor.execute(
                "ALTER TABLE outbox RENAME COLUMN hidden_created_at TO created_at"
            )
    # Сброс gauges после опустошения очереди, включая oldest age.
    with pgsql["db_1"].cursor() as cursor:
        cursor.execute("DELETE FROM outbox WHERE topic=%s", (topic,))
    await service_client.run_periodic_task("outbox-queue-statistics")
    text = (await monitor_client.get("/metrics")).text
    assert metric(text, "outbox_queue_size", topic) == 0
    assert metric(text, "outbox_oldest_message_age_seconds", topic) == 0
    # Не допускаем labels с payload/идентификатором записи/организацией.
    assert not re.search(r"outbox_[^\n]*(message_id|organization_id|payload)=", text)


@pytest.mark.uservice_oneshot(config_hooks=["configure_dispatcher"])
async def test_failed_delivery_retry_and_success_counters(
    api, mockserver, monitor_client
):
    calls = 0
    topic = "shipment.route.project"

    @mockserver.json_handler("/service-token")
    def token(request):
        return {"access_token": "metrics-token"}

    @mockserver.json_handler("/api/internal/v1/shipments/route-projection")
    def deliver(request):
        nonlocal calls
        calls += 1
        if calls == 1:
            return mockserver.make_response(status=503, json={"error": "unavailable"})
        return {"applied": True}

    await api.post(
        "/api/v1/shipments/create",
        {
            "customer_id": str(uuid4()),
            "idempotency_key": str(uuid4()),
        },
        expected=201,
    )
    for _ in range(140):
        text = (await monitor_client.get("/metrics")).text
        if metric(text, "outbox_delivered_total", topic) == 1:
            break
        await asyncio.sleep(0.05)
    else:
        pytest.fail("Повторная доставка не подтверждена")
    assert calls == 2
    assert metric(text, "outbox_attempts_total", topic) == 2
    assert metric(text, "outbox_failed_total", topic) == 1
    assert metric(text, "outbox_retries_total", topic) == 1
    assert metric(text, "outbox_active", topic) == 0
    assert metric(text, "outbox_finished_total", topic) == 2
    assert metric(text, "outbox_duration_microseconds_total", topic) > 0
