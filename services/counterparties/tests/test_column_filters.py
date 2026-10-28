import pytest
from uuid import uuid4
from conftest import ORGANIZATION, OTHER_ORGANIZATION


@pytest.fixture
def records(pgsql):
    with pgsql["db_1"].cursor() as cursor:
        for name, country, roles, organization in [
            ("Альфа", "RU", ["customer"], ORGANIZATION),
            ("Бета", "CN", ["carrier"], ORGANIZATION),
            ("Гамма", "RU", ["carrier", "agent"], ORGANIZATION),
            ("Чужой", "RU", ["carrier"], OTHER_ORGANIZATION),
        ]:
            cursor.execute(
                "INSERT INTO counterparties (organization_id,id,name,country_code,roles) VALUES (%s,%s,%s,%s,%s)",
                (organization, str(uuid4()), name, country, roles),
            )


async def test_roles_use_or_and_other_fields_use_and(api, records):
    result = await api.post(
        "/api/v1/counterparties/list",
        {
            "filters": [
                {"field": "roles", "values": ["customer", "agent"]},
                {"field": "country_code", "values": ["RU"]},
            ],
            "sort": "name",
        },
    )
    assert {item["name"] for item in result["items"]} == {"Альфа", "Гамма"}


async def test_array_facet_uses_same_other_conditions(api, records):
    result = await api.post(
        "/api/v1/counterparties/filter-values",
        {
            "registry": {
                "filters": [
                    {"field": "roles", "values": ["customer"]},
                    {"field": "country_code", "values": ["RU"]},
                ]
            },
            "field": "roles",
        },
    )
    assert {item["value"] for item in result["values"]} == {
        "customer",
        "carrier",
        "agent",
    }


async def test_hidden_or_unknown_sort_is_not_sql(api, records):
    await api.post("/api/v1/counterparties/list", {"sort": "roles"}, expected=400)


@pytest.mark.parametrize('endpoint', ['list', 'filter-values'])
async def test_removed_scalar_filter_is_rejected(api, endpoint):
    body = {'country_code': 'RU'}
    if endpoint == 'filter-values':
        body = {'registry': body, 'field': 'country_code'}
    await api.post(f'/api/v1/counterparties/{endpoint}', body, expected=400)
