SELECT
    ROW(
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
    )::public.shipment_v4,
    {sort_key} AS text_key,
    {date_key} AS date_key
FROM shipments
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
