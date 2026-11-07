SELECT jsonb_strip_nulls(
        (to_jsonb(value) - 'organization_id' - 'conversion' - 'number')
        || jsonb_build_object(
            'number', value.number,
            'converting', value.conversion IS NOT NULL
                AND value.shipment_id IS NULL
                AND value.conversion_error IS NULL
        )
    ),
    ({sort_key})::TEXT AS text_key,
    {date_key} AS date_key
FROM sales_inquiries AS value
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
