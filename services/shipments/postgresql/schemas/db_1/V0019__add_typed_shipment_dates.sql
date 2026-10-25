-- Календарные дни хранятся и передаются как DATE.
CREATE TYPE public.sea_details_v2 AS (
    pol public.TRANSPORT_LOCATION_V1,
    pod public.TRANSPORT_LOCATION_V1,
    eta DATE,
    departure_terminal_id UUID,
    arrival_terminal_id UUID,
    departure_date DATE,
    vessel TEXT,
    voyage TEXT
);

CREATE TYPE public.air_details_v2 AS (
    carrier_id UUID,
    origin_airport public.TRANSPORT_LOCATION_V1,
    destination_airport public.TRANSPORT_LOCATION_V1,
    eta DATE,
    etd DATE,
    departure_date DATE,
    arrival_date DATE,
    flight_number TEXT
);

CREATE TYPE public.shipment_route_segment_v3 AS (
    id UUID,
    type TEXT,
    sea public.SEA_DETAILS_V2,
    air public.AIR_DETAILS_V2
);

CREATE TYPE public.shipment_v4 AS (
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
    route_segments public.SHIPMENT_ROUTE_SEGMENT_V3[]
);

CREATE TYPE public.shipment_data_v3 AS (
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
    route_segments public.SHIPMENT_ROUTE_SEGMENT_V3[]
);

-- Прежние проекции больше не используются. Без CASCADE: зависимости не удаляются неявно.
DROP TYPE public.shipment_v1;
DROP TYPE public.shipment_v2;
DROP TYPE public.shipment_v3;
DROP TYPE public.shipment_data_v1;
DROP TYPE public.shipment_data_v2;
DROP TYPE public.shipment_route_segment_v1;

ALTER TABLE shipments ADD COLUMN typed_route_segments public.shipment_route_segment_v3[];
UPDATE shipments
SET typed_route_segments = ARRAY(
    SELECT jsonb_populate_record(NULL::public.shipment_route_segment_v3, to_jsonb(segment))
    FROM unnest(route_segments) AS segment
);
ALTER TABLE shipments DROP COLUMN route_segments;
ALTER TABLE shipments RENAME COLUMN typed_route_segments TO route_segments;
ALTER TABLE shipments ALTER COLUMN route_segments SET NOT NULL;

DROP TYPE public.shipment_route_segment_v2;
DROP TYPE public.sea_details_v1;
DROP TYPE public.air_details_v1;
