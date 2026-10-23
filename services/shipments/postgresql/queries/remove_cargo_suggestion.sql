UPDATE customer_cargo_suggestions
SET removed = TRUE, removed_at = CURRENT_TIMESTAMP
WHERE organization_id = $1::UUID AND customer_id = $2::UUID AND value = $3 AND NOT removed
RETURNING value;
