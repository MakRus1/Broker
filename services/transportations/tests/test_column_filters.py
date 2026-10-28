from uuid import uuid4
import pytest
from conftest import ORGANIZATION, OTHER_ORGANIZATION


@pytest.fixture
def records(pgsql):
    shipment = str(uuid4())
    with pgsql["db_1"].cursor() as cursor:
        for number, kind, container, owner, hazard, organization in [
            ("ABCU0000001", "container", "40HC", "COC", False, ORGANIZATION),
            ("ABCU0000002", "container", "20DC", "SOC", True, ORGANIZATION),
            (None, "air", None, None, False, ORGANIZATION),
            ("ABCU9999999", "container", "40HC", "COC", False, OTHER_ORGANIZATION),
        ]:
            cursor.execute(
                "INSERT INTO transportations (organization_id,shipment_id,ordinal,type,container_type_code,container_ownership,container_number,hazardous) VALUES (%s,%s,1,%s,%s,%s,%s,%s)",
                (organization, shipment, kind, container, owner, number, hazard),
            )
    return shipment


async def test_combined_values_boolean_and_ownership(api, records):
    result = await api.post(
        "/api/v1/transportations/list",
        {
            "filters": [
                {"field": "container_number", "values": ["abcu0000001", "ABCU0000002"]},
                {"field": "container_ownership", "values": ["SOC"]},
                {"field": "hazardous", "values": ["true"]},
            ],
            "sort": "hazardous",
        },
    )
    assert [row["container_number"] for row in result["items"]] == ["ABCU0000002"]


async def test_shipment_choices_and_other_filters(api, records):
    result = await api.post(
        "/api/v1/transportations/filter-values",
        {
            "registry": {"filters": [{"field": "container_type", "values": ["40HC"]}]},
            "field": "shipment_id",
        },
    )
    assert result["values"] == [{"value": records, "label": records}]
    selected = await api.post(
        "/api/v1/transportations/list",
        {"filters": [{"field": "shipment_id", "values": [records]}]},
    )
    assert len(selected["items"]) == 3


async def test_empty_values_and_literal_search(api, records):
    result = await api.post(
        "/api/v1/transportations/list",
        {"filters": [{"field": "container_number", "empty": True}]},
    )
    assert len(result["items"]) == 1
    choices = await api.post(
        "/api/v1/transportations/filter-values",
        {"registry": {}, "field": "container_number", "query": "%_"},
    )
    assert choices["values"] == []
    await api.post(
        "/api/v1/transportations/list",
        {"filters": [{"field": "hazardous", "values": ["oops"]}]},
        expected=400,
    )


@pytest.mark.parametrize(
    "extra",
    [
        {"candidates": ["not-a-uuid"]},
        {"after": "not-a-uuid"},
    ],
)
async def test_invalid_related_identifiers_do_not_reach_database(api, extra):
    result = await api.post(
        "/api/v1/transportations/filter-values",
        {
            "registry": {},
            "field": "shipment_id",
            **extra,
        },
        expected=400,
    )
    assert result["error"] == "invalid_filter"


@pytest.mark.parametrize('endpoint', ['list', 'filter-values'])
async def test_removed_scalar_filter_is_rejected(api, endpoint):
    body = {'query': 'MSCU'}
    if endpoint == 'filter-values':
        body = {'registry': body, 'field': 'container_number'}
    await api.post(f'/api/v1/transportations/{endpoint}', body, expected=400)
