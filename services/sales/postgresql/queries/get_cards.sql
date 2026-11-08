SELECT jsonb_strip_nulls(to_jsonb(value))
FROM sales_cards AS value
WHERE organization_id = $1::UUID AND id = $2::UUID;
