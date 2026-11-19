CREATE TYPE public.route_plan_v1 AS (
    origin_name TEXT,
    origin_country TEXT,
    origin_address TEXT,
    destination_name TEXT,
    destination_country TEXT,
    destination_address TEXT,
    departure_date DATE,
    arrival_date DATE
);

CREATE TYPE public.shipment_route_segment_v4 AS (
    id UUID,
    type TEXT,
    sea public.SEA_DETAILS_V2,
    air public.AIR_DETAILS_V2,
    plan public.ROUTE_PLAN_V1
);

CREATE TYPE public.shipment_v5 AS (
    id UUID,
    revision BIGINT,
    number TEXT,
    customer_id UUID,
    reference TEXT,
    shipment_date DATE,
    recipient_id UUID,
    logist TEXT,
    cargo_description TEXT,
    status TEXT,
    incoterms TEXT,
    carriage_terms TEXT,
    loading TEXT,
    delivery_city TEXT,
    delivery_country TEXT,
    delivery_address TEXT,
    route_segments public.SHIPMENT_ROUTE_SEGMENT_V4[]
);

CREATE TYPE public.shipment_data_v4 AS (
    number TEXT,
    customer_id UUID,
    reference TEXT,
    shipment_date DATE,
    recipient_id UUID,
    logist TEXT,
    cargo_description TEXT,
    status TEXT,
    incoterms TEXT,
    carriage_terms TEXT,
    loading TEXT,
    delivery_city TEXT,
    delivery_country TEXT,
    delivery_address TEXT,
    route_segments public.SHIPMENT_ROUTE_SEGMENT_V4[]
);

ALTER TABLE shipments ADD COLUMN planned_route_segments public.shipment_route_segment_v4[];
UPDATE shipments SET planned_route_segments = ARRAY(
    SELECT ROW(segment.id, segment.type, segment.sea, segment.air, NULL)::public.shipment_route_segment_v4
    FROM unnest(route_segments) AS segment
);
ALTER TABLE shipments DROP COLUMN route_segments;
ALTER TABLE shipments RENAME COLUMN planned_route_segments TO route_segments;
ALTER TABLE shipments ALTER COLUMN route_segments SET NOT NULL;
