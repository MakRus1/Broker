CREATE TYPE public.route_booking_v1 AS (
    number TEXT,
    status TEXT,
    placed_date DATE,
    confirmed_date DATE
);

CREATE TYPE public.bill_of_lading_v1 AS (
    number TEXT,
    date DATE,
    leg TEXT
);

CREATE TYPE public.air_waybill_v1 AS (
    number TEXT,
    date DATE
);

CREATE TYPE public.route_document_event_v1 AS (
    date DATE
);

CREATE TYPE public.sea_details_v4 AS (
    pol public.TRANSPORT_LOCATION_V1,
    pod public.TRANSPORT_LOCATION_V1,
    eta DATE,
    departure_terminal_id UUID,
    arrival_terminal_id UUID,
    departure_date DATE,
    vessel TEXT,
    voyage TEXT,
    booking public.route_booking_v1,
    documents public.bill_of_lading_v1[],
    original_received public.route_document_event_v1,
    telex_release public.route_document_event_v1
);

CREATE TYPE public.air_details_v4 AS (
    carrier_id UUID,
    origin_airport public.TRANSPORT_LOCATION_V1,
    destination_airport public.TRANSPORT_LOCATION_V1,
    eta DATE,
    etd DATE,
    departure_date DATE,
    arrival_date DATE,
    flight_number TEXT,
    documents public.air_waybill_v1[]
);

CREATE TYPE public.shipment_route_segment_v6 AS (
    id UUID,
    type TEXT,
    sea public.sea_details_v4,
    air public.air_details_v4,
    plan public.ROUTE_PLAN_V1
);

CREATE TYPE public.shipment_v7 AS (
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
    route_segments public.shipment_route_segment_v6[]
);

CREATE TYPE public.shipment_data_v6 AS (
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
    route_segments public.shipment_route_segment_v6[]
);

ALTER TABLE shipments ADD COLUMN documented_route_segments public.shipment_route_segment_v6[];
UPDATE shipments SET documented_route_segments = ARRAY(
    SELECT jsonb_populate_record(NULL::public.shipment_route_segment_v6, to_jsonb(shipments.route_segments[position]))
    FROM generate_subscripts(route_segments, 1) AS position
    ORDER BY position
);
ALTER TABLE shipments DROP COLUMN route_segments;
ALTER TABLE shipments RENAME COLUMN documented_route_segments TO route_segments;
ALTER TABLE shipments ALTER COLUMN route_segments SET NOT NULL;
