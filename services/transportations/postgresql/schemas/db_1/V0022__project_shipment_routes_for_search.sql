-- Версия принадлежит маршруту поставки; доставка повторов и старых версий безопасна.
CREATE TABLE shipment_route_projections (
    organization_id UUID NOT NULL,
    shipment_id UUID NOT NULL,
    revision BIGINT NOT NULL,
    PRIMARY KEY (organization_id, shipment_id)
);

CREATE TABLE shipment_route_segments (
    organization_id UUID NOT NULL,
    shipment_id UUID NOT NULL,
    id UUID NOT NULL,
    mode TEXT NOT NULL,
    document JSONB NOT NULL,
    PRIMARY KEY (organization_id, shipment_id, id)
);
CREATE INDEX shipment_route_segments_mode_idx
    ON shipment_route_segments (organization_id, mode, shipment_id);

-- Часто используемые точные фильтры станций: одинаковый индекс обслуживает все группы.
CREATE INDEX route_segments_origin_station_idx ON route_segments (
    organization_id,
    ((document #>> '{rail,origin_station,id}')::UUID),
    transportation_id,
    id
);
CREATE INDEX route_segments_destination_station_idx ON route_segments (
    organization_id,
    ((document #>> '{rail,destination_station,id}')::UUID),
    transportation_id,
    id
);

-- Оценки частот общих портов/аэропортов нужны планировщику для порядка групп EXISTS.
-- Это статистика, а не индекс на каждый будущий атрибут JSON.
CREATE STATISTICS shipment_route_location_stats ON
    ((document #>> '{sea,pol,id}')::UUID),
    ((document #>> '{sea,pod,id}')::UUID),
    ((document #>> '{air,origin_airport,id}')::UUID),
    ((document #>> '{air,destination_airport,id}')::UUID)
FROM shipment_route_segments;
