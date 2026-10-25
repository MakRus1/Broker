SELECT ROW(
    id,
    revision,
    number,
    customer_id,
    reference,
    shipment_date,
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
)::public.shipment_v4
FROM shipments
WHERE organization_id = $1::UUID AND id = ANY($2::UUID[]);
