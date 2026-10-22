UPDATE cargo_suggestions SET removed = TRUE, removed_at = CURRENT_TIMESTAMP
WHERE organization_id = $1::UUID AND value = $2 AND NOT removed
RETURNING value;
