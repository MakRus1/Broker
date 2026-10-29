"""Группы проверяются на одном участке до LIMIT; море/авиа берутся из проекции поставки."""

import json
from uuid import uuid4
import pytest
from conftest import ORGANIZATION, OTHER_ORGANIZATION

LIST = "/api/v1/transportations/list"
VALUES = "/api/v1/transportations/route-filter-values"
PROJECTION = "/api/internal/v1/shipments/route-projection"


@pytest.fixture
def routes(pgsql, shipment_routes):
    station_a, station_b, port, airport = [str(uuid4()) for _ in range(4)]
    point = lambda identifier, name: {
        "id": identifier,
        "name": name,
        "country_code": "RU",
        "esr_code": "123456",
    }
    sea_id, rail_id, air_id = [str(uuid4()) for _ in range(3)]
    shipments = [str(uuid4()) for _ in range(3)]
    ids = [str(uuid4()) for _ in range(3)]

    def source(mode, identifier, **details):
        return {"id": identifier, "type": mode, **details}

    sea = {
        "pol": {
            "id": port,
            "name": "Владивосток",
            "country_code": "RU",
            "kind": "sea_port",
            "code": "RUVVO",
        }
    }
    air = {
        "destination_airport": {
            "id": airport,
            "name": "Шереметьево",
            "country_code": "RU",
            "kind": "airport",
            "code": "SVO",
        }
    }
    structures = [
        [source("sea", sea_id, sea=sea), source("rail", rail_id)],
        [source("international_rail", rail_id), source("air", air_id, air=air)],
        [source("rail", rail_id)],
    ]
    with pgsql["db_1"].cursor() as cursor:
        for index, (shipment, identifier, structure) in enumerate(
            zip(shipments, ids, structures)
        ):
            shipment_routes.set(shipment, structure)
            cursor.execute(
                "INSERT INTO transportations (organization_id,id,shipment_id,ordinal,type,hazardous) VALUES (%s,%s,%s,1,'rail',false)",
                (ORGANIZATION, identifier, shipment),
            )
            for stage in structure:
                cursor.execute(
                    "INSERT INTO shipment_route_segments (organization_id,shipment_id,id,mode,document) VALUES (%s,%s,%s,%s,%s)",
                    (
                        ORGANIZATION,
                        shipment,
                        stage["id"],
                        stage["type"],
                        json.dumps(stage),
                    ),
                )
            local = {
                "id": rail_id,
                "mode": (
                    structure[-1]["type"]
                    if index == 2
                    else structure[0 if index == 1 else 1]["type"]
                ),
                "rail": {
                    "origin_station": point(station_a, "Альфа"),
                    "destination_station": point(station_b, "Бета"),
                },
            }
            cursor.execute(
                "INSERT INTO route_segments (organization_id,transportation_id,id,position,document) VALUES (%s,%s,%s,1,%s)",
                (ORGANIZATION, identifier, rail_id, json.dumps(local)),
            )
    return {
        "ids": ids,
        "shipments": shipments,
        "a": station_a,
        "b": station_b,
        "port": port,
        "airport": airport,
        "sea_id": sea_id,
    }


async def test_conditions_must_match_same_segment(api, routes):
    # Первый маршрут содержит море и ЖД до Беты; станция не должна приписываться морю.
    result = await api.post(
        LIST,
        {
            "route_filters": [
                {
                    "filters": [
                        {"field": "mode", "values": ["sea"]},
                        {"field": "destination_station", "values": [routes["b"]]},
                    ]
                }
            ]
        },
    )
    assert result["items"] == []
    result = await api.post(
        LIST,
        {
            "route_filters": [
                {
                    "filters": [
                        {"field": "mode", "values": ["rail"]},
                        {"field": "destination_station", "values": [routes["b"]]},
                    ]
                }
            ]
        },
    )
    assert {value["id"] for value in result["items"]} == {
        routes["ids"][0],
        routes["ids"][2],
    }


async def test_multiple_groups_and_main_conditions(api, routes):
    body = {
        "route_filters": [
            {
                "filters": [
                    {"field": "mode", "values": ["sea"]},
                    {"field": "pol", "values": [routes["port"]]},
                ]
            },
            {
                "filters": [
                    {"field": "mode", "values": ["rail"]},
                    {"field": "destination_station", "values": [routes["b"]]},
                ]
            },
        ],
        "limit": 1,
    }
    result = await api.post(LIST, body)
    assert [value["id"] for value in result["items"]] == [routes["ids"][0]]
    assert "next_cursor" not in result
    body["filters"] = [{"field": "id", "values": [routes["ids"][1]]}]
    assert (await api.post(LIST, body))["items"] == []


async def test_airport_and_port_use_inherited_projection(api, routes):
    result = await api.post(
        LIST,
        {
            "route_filters": [
                {
                    "filters": [
                        {"field": "destination_airport", "values": [routes["airport"]]}
                    ]
                }
            ]
        },
    )
    assert [value["id"] for value in result["items"]] == [routes["ids"][1]]
    # Удалённый порт исчезает из поиска вместе с новой версией маршрута.
    await api.post(
        PROJECTION,
        {
            "organization_id": ORGANIZATION,
            "shipment_id": routes["shipments"][0],
            "revision": 2,
            "route_segments": [],
        },
    )
    assert (
        await api.post(
            LIST,
            {
                "route_filters": [
                    {"filters": [{"field": "pol", "values": [routes["port"]]}]}
                ]
            },
        )
    )["items"] == []


async def test_values_respect_other_conditions_and_page(api, routes):
    registry = {
        "route_filters": [
            {
                "filters": [
                    {"field": "mode", "values": ["sea"]},
                    {"field": "pol", "values": [routes["port"]]},
                ]
            }
        ]
    }
    result = await api.post(
        VALUES, {"registry": registry, "field": "mode", "group": 0, "limit": 1}
    )
    assert result["values"] == [{"value": "sea", "label": "sea"}]
    result = await api.post(
        VALUES, {"registry": {}, "field": "destination_station", "query": "Бета"}
    )
    assert result["values"] == [{"value": routes["b"], "label": "Бета · 123456"}]
    assert (
        await api.post(
            VALUES, {"registry": {}, "field": "destination_station"}, user="other"
        )
    )["values"] == []


async def test_route_cursor_scope_and_organization(api, routes):
    first = await api.post(
        LIST,
        {
            "limit": 1,
            "route_filters": [
                {"filters": [{"field": "destination_station", "values": [routes["b"]]}]}
            ],
        },
    )
    assert first.get("next_cursor")
    await api.post(
        LIST,
        {
            "cursor": first["next_cursor"],
            "route_filters": [{"filters": [{"field": "mode", "values": ["sea"]}]}],
        },
        expected=400,
    )
    result = await api.post(
        LIST,
        {"route_filters": [{"filters": [{"field": "mode", "values": ["rail"]}]}]},
        user="other",
    )
    assert result["items"] == []


@pytest.mark.parametrize(
    "filters",
    [
        [{"field": "mode; SELECT 1", "values": ["sea"]}],
        [{"field": "pol", "values": ["not-uuid"]}],
        [{"field": "mode", "values": ["sea"], "query": "sea"}],
    ],
)
async def test_invalid_group_conditions(api, filters):
    await api.post(LIST, {"route_filters": [{"filters": filters}]}, expected=400)


async def test_projection_is_idempotent_and_rejects_old_versions(api, routes, pgsql):
    body = {
        "organization_id": ORGANIZATION,
        "shipment_id": routes["shipments"][2],
        "revision": 10,
        "route_segments": [{"id": str(uuid4()), "type": "air"}],
    }
    assert (await api.post(PROJECTION, body))["applied"]
    assert not (await api.post(PROJECTION, body))["applied"]
    assert not (
        await api.post(PROJECTION, {**body, "revision": 9, "route_segments": []})
    )["applied"]
    with pgsql["db_1"].cursor() as cursor:
        cursor.execute(
            "SELECT mode FROM shipment_route_segments WHERE organization_id=%s AND shipment_id=%s",
            (ORGANIZATION, body["shipment_id"]),
        )
        assert cursor.fetchall() == [("air",)]
    await api.post(PROJECTION, body, user="other", expected=403)


async def test_route_values_cursor_covers_distinct_modes(api, routes):
    first = await api.post(VALUES, {"registry": {}, "field": "mode", "limit": 2})
    second = await api.post(
        VALUES,
        {"registry": {}, "field": "mode", "limit": 2, "after": first["next_after"]},
    )
    assert [value["value"] for value in first["values"] + second["values"]] == [
        "air",
        "international_rail",
        "rail",
        "sea",
    ]
    assert "next_after" not in second


@pytest.mark.parametrize(
    "field,key,label",
    [
        ("pol", "port", "Владивосток · RUVVO"),
        ("pod", None, None),
        ("origin_airport", None, None),
        ("destination_airport", "airport", "Шереметьево · SVO"),
    ],
)
async def test_location_values_catalogue(api, routes, field, key, label):
    result = await api.post(VALUES, {"registry": {}, "field": field})
    expected = [] if key is None else [{"value": routes[key], "label": label}]
    assert result["values"] == expected
