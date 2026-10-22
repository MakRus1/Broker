WITH expired AS (
    SELECT organization_id, value
    FROM cargo_suggestions
    WHERE (NOT removed AND used_at < CURRENT_TIMESTAMP - $1::INTEGER * INTERVAL '1 day')
        OR (removed AND removed_at < CURRENT_TIMESTAMP - $2::INTEGER * INTERVAL '1 day')
    ORDER BY COALESCE(removed_at, used_at), organization_id, value
    LIMIT $3
    FOR UPDATE SKIP LOCKED
)
DELETE FROM cargo_suggestions AS suggestion
USING expired
WHERE suggestion.organization_id = expired.organization_id
    AND suggestion.value = expired.value
RETURNING suggestion.value;
