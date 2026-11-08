SELECT stage_id::TEXT
FROM sales_cards
WHERE organization_id = $1::UUID AND id = $2::UUID
FOR UPDATE;
