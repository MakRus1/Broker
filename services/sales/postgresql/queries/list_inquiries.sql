SELECT jsonb_strip_nulls(
        (to_jsonb(value) - 'conversion')
        || jsonb_build_object(
            'converting', conversion IS NOT NULL
                AND shipment_id IS NULL
                AND conversion_error IS NULL
        )
    ),
    ({sort_key})::TEXT AS text_key,
    {date_key} AS date_key
FROM sales_inquiries AS value
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
