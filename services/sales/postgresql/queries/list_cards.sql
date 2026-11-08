SELECT jsonb_strip_nulls(to_jsonb(sales_cards)),
    ({sort_key})::TEXT AS text_key,
    {date_key} AS date_key
FROM sales_cards
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
