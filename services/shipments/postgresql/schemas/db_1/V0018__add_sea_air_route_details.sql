CREATE TABLE route_locations (
    id UUID PRIMARY KEY,
    kind TEXT NOT NULL,
    name TEXT NOT NULL,
    country_code TEXT NOT NULL,
    code TEXT,
    archived BOOLEAN NOT NULL DEFAULT FALSE
);

CREATE UNIQUE INDEX route_locations_code
ON route_locations (kind, country_code, code) WHERE code IS NOT NULL;

CREATE TYPE public.transport_location_v1 AS (
    id UUID,
    kind TEXT,
    name TEXT,
    country_code TEXT,
    code TEXT
);

CREATE TYPE public.sea_details_v1 AS (
    pol public.TRANSPORT_LOCATION_V1,
    pod public.TRANSPORT_LOCATION_V1,
    eta TEXT,
    departure_terminal_id UUID,
    arrival_terminal_id UUID,
    departure_date TEXT,
    vessel TEXT,
    voyage TEXT
);

CREATE TYPE public.air_details_v1 AS (
    carrier_id UUID,
    origin_airport public.TRANSPORT_LOCATION_V1,
    destination_airport public.TRANSPORT_LOCATION_V1,
    eta TEXT,
    etd TEXT,
    departure_date TEXT,
    arrival_date TEXT,
    flight_number TEXT
);

CREATE TYPE public.shipment_route_segment_v2 AS (
    id UUID,
    type TEXT,
    sea public.SEA_DETAILS_V1,
    air public.AIR_DETAILS_V1
);

CREATE TYPE public.shipment_v3 AS (
    id UUID,
    revision BIGINT,
    number TEXT,
    customer_id UUID,
    reference TEXT,
    shipment_date TEXT,
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
    route_segments public.SHIPMENT_ROUTE_SEGMENT_V2[]
);

CREATE TYPE public.shipment_data_v2 AS (
    number TEXT,
    customer_id UUID,
    reference TEXT,
    shipment_date TEXT,
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
    route_segments public.SHIPMENT_ROUTE_SEGMENT_V2[]
);

ALTER TABLE shipments ADD COLUMN route_segments_new public.shipment_route_segment_v2[];

UPDATE shipments
SET route_segments_new = ARRAY(
    SELECT ROW(segment.id, segment.type, NULL, NULL)::public.shipment_route_segment_v2
    FROM unnest(route_segments) AS segment
);

ALTER TABLE shipments DROP COLUMN route_segments;
ALTER TABLE shipments RENAME COLUMN route_segments_new TO route_segments;
ALTER TABLE shipments ALTER COLUMN route_segments SET NOT NULL;
