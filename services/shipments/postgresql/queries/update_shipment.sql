UPDATE shipments AS s
SET
    number = card.number,
    customer_id = card.customer_id,
    reference = card.reference,
    shipment_date = card.shipment_date::DATE,
    recipient_id = card.recipient_id,
    logist = card.logist,
    cargo_description = card.cargo_description,
    status = card.status,
    incoterms = card.incoterms,
    carriage_terms = card.carriage_terms,
    loading = card.loading,
    delivery_city = card.delivery_city,
    delivery_country = card.delivery_country,
    delivery_address = card.delivery_address,
    route_segments = card.route_segments,
    revision = s.revision + 1
FROM (SELECT ($4::public.shipment_data_v1).*) AS card
WHERE s.organization_id = $1::UUID AND s.id = $2::UUID AND s.revision = $3
RETURNING ROW(
    s.id,
    s.revision,
    s.number,
    s.customer_id,
    s.reference,
    s.shipment_date::TEXT,
    s.recipient_id,
    s.logist,
    s.cargo_description,
    s.status,
    s.incoterms,
    s.carriage_terms,
    s.loading,
    s.delivery_city,
    s.delivery_country,
    s.delivery_address,
    s.route_segments
)::public.shipment_v2;
