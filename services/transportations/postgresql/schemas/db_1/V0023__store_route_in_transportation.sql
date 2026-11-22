-- Маршрут — часть записи перевозки. Переносим документы без изменения данных и порядка.
ALTER TABLE transportations ADD COLUMN route_segments JSONB NOT NULL DEFAULT '[]'::JSONB
    CHECK (jsonb_typeof(route_segments) = 'array');

UPDATE transportations AS transportation
SET route_segments = route.documents
FROM (
    SELECT organization_id, transportation_id,
           jsonb_agg(document ORDER BY position, id) AS documents
    FROM route_segments
    GROUP BY organization_id, transportation_id
) AS route
WHERE transportation.organization_id = route.organization_id
  AND transportation.id = route.transportation_id;

CREATE TYPE public.transportation_v5 AS (
    id UUID,
    revision BIGINT,
    shipment_id UUID,
    item_id UUID,
    operation_id UUID,
    container_type TEXT,
    ownership TEXT,
    container_number TEXT,
    type TEXT,
    hazardous BOOLEAN,
    route_segments JSONB
);

DROP TABLE route_segments;

-- Поиск станций индексируется по полю перевозки, без отдельной таблицы участков.
CREATE FUNCTION public.transportation_route_station_ids(route JSONB, point TEXT)
RETURNS UUID[] LANGUAGE SQL IMMUTABLE PARALLEL SAFE AS $$
    SELECT COALESCE(array_agg((segment #>> ARRAY['rail', point, 'id'])::UUID)
        FILTER (WHERE segment #>> ARRAY['rail', point, 'id'] IS NOT NULL), ARRAY[]::UUID[])
    FROM jsonb_array_elements(route) AS segment;
$$;
CREATE INDEX transportations_origin_station_ids_idx ON transportations
    USING GIN (public.transportation_route_station_ids(route_segments, 'origin_station'));
CREATE INDEX transportations_destination_station_ids_idx ON transportations
    USING GIN (public.transportation_route_station_ids(route_segments, 'destination_station'));
