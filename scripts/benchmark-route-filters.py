"""Нагрузка маршрутов для общего benchmark-column-filters.py --route."""

from uuid import UUID

ORG = "00000000-0000-0000-0000-000000000001"
STATION = str(UUID(int=100))
PORT = str(UUID(int=200))


def seed(rows):
    return f"""
    INSERT INTO shipment_route_projections (organization_id, shipment_id, revision)
    SELECT DISTINCT organization_id, shipment_id, 1 FROM transportations;

    INSERT INTO shipment_route_segments (organization_id, shipment_id, id, mode, document)
    SELECT organization_id, shipment_id, md5(shipment_id::text || ':' || mode)::uuid, mode,
           jsonb_build_object('id', md5(shipment_id::text || ':' || mode)::uuid, 'type', mode,
               'sea', CASE WHEN mode = 'sea' THEN jsonb_build_object('pol',
                   jsonb_build_object('id', '{PORT}', 'name', 'Владивосток', 'country_code', 'RU', 'kind', 'sea_port')) ELSE NULL END)
    FROM shipment_route_projections CROSS JOIN unnest(ARRAY['sea', 'rail', 'air']) AS mode;

    INSERT INTO route_segments (organization_id, transportation_id, id, position, document)
    SELECT t.organization_id, t.id, shared.id, 1,
           jsonb_build_object('id',shared.id,'mode','rail','rail',jsonb_build_object('destination_station',
               jsonb_build_object('id', CASE WHEN t.hazardous THEN '{STATION}' ELSE md5(t.id::text)::text END,
                   'name',CASE WHEN t.hazardous THEN 'Редкая станция' ELSE 'Обычная станция' END,
                   'country_code','RU','esr_code','123456')))
    FROM transportations AS t JOIN shipment_route_segments AS shared
      ON shared.organization_id=t.organization_id AND shared.shipment_id=t.shipment_id AND shared.mode='rail';

    VACUUM ANALYZE shipment_route_segments;
    VACUUM ANALYZE route_segments;
    """


def cases():
    return [
        (
            "route_mode",
            {"route_filters": [{"filters": [{"field": "mode", "values": ["rail"]}]}]},
            [["rail"]],
            False,
        ),
        (
            "route_station",
            {
                "route_filters": [
                    {
                        "filters": [
                            {"field": "mode", "values": ["rail"]},
                            {"field": "destination_station", "values": [STATION]},
                        ]
                    }
                ]
            },
            [["rail"], [STATION]],
            False,
        ),
        (
            "route_two_groups",
            {
                "route_filters": [
                    {
                        "filters": [
                            {"field": "mode", "values": ["sea"]},
                            {"field": "pol", "values": [PORT]},
                        ]
                    },
                    {
                        "filters": [
                            {"field": "mode", "values": ["rail"]},
                            {"field": "destination_station", "values": [STATION]},
                        ]
                    },
                ]
            },
            [["sea"], [PORT], ["rail"], [STATION]],
            False,
        ),
        (
            "route_no_match",
            {
                "route_filters": [
                    {
                        "filters": [
                            {
                                "field": "destination_station",
                                "values": [str(UUID(int=999))],
                            }
                        ]
                    }
                ]
            },
            [[str(UUID(int=999))]],
            False,
        ),
        (
            "route_values",
            {"request": {}, "facet": "destination_station", "route_group": 0},
            [],
            False,
        ),
    ]
