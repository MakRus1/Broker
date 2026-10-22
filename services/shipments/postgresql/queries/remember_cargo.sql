INSERT INTO cargo_suggestions AS c (organization_id, value, search_value)
VALUES ($1::UUID, $2, $3)
ON CONFLICT (organization_id, value) DO UPDATE SET used_at = CURRENT_TIMESTAMP
WHERE NOT c.removed;
