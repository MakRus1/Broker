INSERT INTO customer_cargo_suggestions AS c (organization_id, customer_id, value, search_value)
VALUES ($1::UUID, $2::UUID, $3, $4)
ON CONFLICT (organization_id, customer_id, value) DO UPDATE SET used_at = CURRENT_TIMESTAMP
WHERE NOT c.removed;
