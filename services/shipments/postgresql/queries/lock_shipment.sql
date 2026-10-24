SELECT ROW(
    id,
    revision,
    number,
    customer_id,
    reference,
    shipment_date::TEXT,
    recipient_id,
    logist,
    cargo_description,
    status,
    incoterms,
    carriage_terms,
    loading,
    delivery_city,
    delivery_country,
    delivery_address,
    route_segments
)::public.shipment_v3
FROM shipments
WHERE organization_id = $1::UUID AND id = $2::UUID
FOR UPDATE;
