CREATE TYPE public.shipment_route_segment_v1 AS (
    id UUID,
    type TEXT
);

-- Сохраняем отображаемые номера существующих поставок в единственном поле.
ALTER TABLE shipments ALTER COLUMN number DROP IDENTITY;
ALTER TABLE shipments ALTER COLUMN number TYPE TEXT USING ('S-' || number::TEXT);

ALTER TABLE shipments
    ADD COLUMN shipment_date DATE NOT NULL DEFAULT CURRENT_DATE,
    ADD COLUMN recipient_id UUID,
    ADD COLUMN logist TEXT,
    ADD COLUMN cargo_description TEXT,
    ADD COLUMN status TEXT NOT NULL DEFAULT 'Новая',
    ADD COLUMN incoterms TEXT,
    ADD COLUMN carriage_terms TEXT,
    ADD COLUMN loading TEXT,
    ADD COLUMN delivery_city TEXT,
    ADD COLUMN delivery_country TEXT,
    ADD COLUMN delivery_address TEXT,
    ADD COLUMN route_segments public.shipment_route_segment_v1[] NOT NULL DEFAULT '{}';

CREATE TYPE public.shipment_v2 AS (
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
    route_segments public.shipment_route_segment_v1[]
);

-- Только записываемые поля, без серверных id и revision.
CREATE TYPE public.shipment_data_v1 AS (
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
    route_segments public.shipment_route_segment_v1[]
);
