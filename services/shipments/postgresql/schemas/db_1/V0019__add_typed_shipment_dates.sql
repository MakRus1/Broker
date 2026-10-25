-- Календарная дата в актуальных проекциях хранения.
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
    route_segments public.SHIPMENT_ROUTE_SEGMENT_V2[]
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
    route_segments public.SHIPMENT_ROUTE_SEGMENT_V2[]
);

-- Прежние проекции больше не используются. Без CASCADE: зависимости не удаляются неявно.
DROP TYPE public.shipment_v1;
DROP TYPE public.shipment_v2;
DROP TYPE public.shipment_v3;
DROP TYPE public.shipment_data_v1;
DROP TYPE public.shipment_data_v2;
DROP TYPE public.shipment_route_segment_v1;
