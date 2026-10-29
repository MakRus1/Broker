import asyncio
from uuid import uuid4
import pytest
from conftest import ORGANIZATION


@pytest.mark.uservice_oneshot(config_hooks=["configure_dispatcher"])
async def test_projection_retries_lost_response_and_preserves_new_revision(
    api, mockserver, pgsql
):
    received = []

    @mockserver.json_handler("/service-token")
    def token(request):
        return {"access_token": "route-sync-token"}

    @mockserver.json_handler("/api/internal/v1/shipments/route-projection")
    def receive(request):
        received.append(request.json)
        if len(received) == 1:
            return mockserver.make_response(
                status=503, json={"error": "unavailable", "message": "Retry"}
            )
        return {"applied": True}

    shipment = (
        await api.post(
            "/api/v1/shipments/create",
            {
                "customer_id": str(uuid4()),
                "idempotency_key": str(uuid4()),
                "route_segments": [{"id": str(uuid4()), "type": "sea"}],
            },
            expected=201,
        )
    )["shipment"]
    for _ in range(100):
        if received:
            break
        await asyncio.sleep(0.05)
    assert received
    assert received[0]["organization_id"] == ORGANIZATION
    assert received[0]["route_segments"] == shipment["route_segments"]
    updated = (
        await api.post(
            "/api/v1/shipments/update",
            {
                "shipment_id": shipment["id"],
                "expected_revision": shipment["revision"],
                "route_segments": [{"id": str(uuid4()), "type": "air"}],
            },
        )
    )["shipment"]
    for _ in range(120):
        if any(value["revision"] == updated["revision"] for value in received):
            break
        await asyncio.sleep(0.05)
    assert received[-1]["revision"] == updated["revision"]
    assert received[-1]["route_segments"] == updated["route_segments"]
    for _ in range(40):
        with pgsql["db_1"].cursor() as cursor:
            cursor.execute(
                "SELECT count(*) FROM route_projection_outbox WHERE shipment_id=%s",
                (shipment["id"],),
            )
            if cursor.fetchone()[0] == 0:
                break
        await asyncio.sleep(0.05)
    else:
        pytest.fail("Подтверждённая проекция осталась в outbox")


@pytest.mark.uservice_oneshot(config_hooks=["configure_dispatcher"])
async def test_projection_delivers_postgres_composite_backfill(api, mockserver, pgsql):
    received = []

    @mockserver.json_handler("/service-token")
    def token(request):
        return {"access_token": "route-sync-token"}

    @mockserver.json_handler("/api/internal/v1/shipments/route-projection")
    def receive(request):
        received.append(request.json)
        return {"applied": True}

    shipment = (
        await api.post(
            "/api/v1/shipments/create",
            {
                "customer_id": str(uuid4()),
                "idempotency_key": str(uuid4()),
                "route_segments": [{"id": str(uuid4()), "type": "sea"}],
            },
            expected=201,
        )
    )["shipment"]
    # Тот же формат, который миграция получает из PostgreSQL composite:
    # необязательные sea/air присутствуют со значением null.
    with pgsql["db_1"].cursor() as cursor:
        cursor.execute(
            """
            INSERT INTO route_projection_outbox
                (organization_id, shipment_id, revision, route_segments)
            SELECT organization_id, id, 100, to_jsonb(route_segments)
            FROM shipments WHERE id = %s
            ON CONFLICT (organization_id, shipment_id) DO UPDATE
            SET revision = 100, route_segments = EXCLUDED.route_segments,
                next_attempt_at = now(), lease_until = NULL, lease_token = NULL
            """,
            (shipment["id"],),
        )
    for _ in range(100):
        projected = [value for value in received if value["revision"] == 100]
        if projected:
            break
        await asyncio.sleep(0.05)
    else:
        pytest.fail("Начальная проекция PostgreSQL composite не доставлена")
    assert projected[0]["route_segments"] == shipment["route_segments"]
