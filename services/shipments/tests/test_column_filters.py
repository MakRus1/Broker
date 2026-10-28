from uuid import uuid4
import pytest
from conftest import ORGANIZATION, OTHER_ORGANIZATION


@pytest.fixture
def records(pgsql):
    with pgsql["db_1"].cursor() as cursor:
        for number, status, day, organization in [
            ("S-2", "Новая", "2026-01-02", ORGANIZATION),
            ("S-10", "В работе", "2026-01-10", ORGANIZATION),
            ("S-11", "Новая", "2026-01-11", ORGANIZATION),
            ("S-999", "Новая", "2026-01-12", OTHER_ORGANIZATION),
        ]:
            cursor.execute(
                "INSERT INTO shipments (organization_id,number,customer_id,shipment_date,status,route_segments) VALUES (%s,%s,%s,%s,%s,'{}')",
                (organization, number, str(uuid4()), day, status),
            )


async def test_multiple_numbers_and_natural_order(api, records):
    result = await api.post(
        "/api/v1/shipments/list",
        {"sort": "number", "filters": [{"field": "number", "values": ["S-10", "s-2"]}]},
    )
    assert [row["number"] for row in result["items"]] == ["S-2", "S-10"]


async def test_facets_exclude_only_their_own_condition_and_have_cursor(api, records):
    registry = {
        "sort": "number",
        "filters": [
            {"field": "number", "values": ["s-2"]},
            {"field": "status", "values": ["Новая"]},
        ],
    }
    first = await api.post(
        "/api/v1/shipments/filter-values",
        {"registry": registry, "field": "number", "limit": 1},
    )
    assert first["values"] == [{"value": "s-2", "label": "S-2"}]
    second = await api.post(
        "/api/v1/shipments/filter-values",
        {
            "registry": registry,
            "field": "number",
            "limit": 1,
            "after": first["next_after"],
        },
    )
    assert second["values"] == [{"value": "s-11", "label": "S-11"}]
    assert "next_after" not in second
    assert any(
        field["id"] == "number" and field["sortable"] and field["case_insensitive"]
        for field in first["fields"]
    )


async def test_date_range_is_typed_and_composes_with_values(api, records):
    result = await api.post(
        "/api/v1/shipments/list",
        {
            "filters": [
                {
                    "field": "shipment_date",
                    "date_from": "2026-01-05",
                    "date_to": "2026-01-11",
                },
                {"field": "status", "values": ["Новая"]},
            ]
        },
    )
    assert [row["number"] for row in result["items"]] == ["S-11"]
    await api.post(
        "/api/v1/shipments/list",
        {"filters": [{"field": "shipment_date", "date_from": "2026-02-30"}]},
        expected=400,
    )


@pytest.mark.parametrize(
    "body",
    [
        {"filters": [{"field": "number; DROP TABLE shipments", "values": ["x"]}]},
        {"sort": "number; DROP TABLE shipments"},
        {"filters": [{"field": "number", "values": ["s-2"], "query": "s"}]},
        {
            "filters": [
                {
                    "field": "shipment_date",
                    "date_from": "2026-02-01",
                    "date_to": "2026-01-01",
                }
            ]
        },
    ],
)
async def test_invalid_filters_are_rejected_before_sql(api, records, body):
    result = await api.post("/api/v1/shipments/list", body, expected=400)
    assert result["error"] == "invalid_filter"


async def test_cursor_cannot_be_reused_with_another_filter(api, records):
    first = await api.post("/api/v1/shipments/list", {"sort": "number", "limit": 1})
    await api.post(
        "/api/v1/shipments/list",
        {
            "sort": "number",
            "limit": 1,
            "cursor": first["next_cursor"],
            "filters": [{"field": "status", "values": ["Новая"]}],
        },
        expected=400,
    )
