SELECT jsonb_strip_nulls(to_jsonb(value) - 'organization_id'),
    ({sort_key})::TEXT AS text_key,
    {date_key} AS date_key
FROM sales_cards AS value
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
