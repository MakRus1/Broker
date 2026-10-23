WITH expired AS (
    SELECT id
    FROM customer_cargo_suggestions
    WHERE (NOT removed AND used_at < CURRENT_TIMESTAMP - $1::INTEGER * INTERVAL '1 day')
        OR (removed AND removed_at < CURRENT_TIMESTAMP - $2::INTEGER * INTERVAL '1 day')
    ORDER BY COALESCE(removed_at, used_at), id
    LIMIT $3
    FOR UPDATE SKIP LOCKED
), legacy_expired AS (
    -- Обе таблицы делят общий лимит; старые поды могут ещё записывать общие подсказки.
    SELECT organization_id, value
    FROM cargo_suggestions
    WHERE (NOT removed AND used_at < CURRENT_TIMESTAMP - $1::INTEGER * INTERVAL '1 day')
        OR (removed AND removed_at < CURRENT_TIMESTAMP - $2::INTEGER * INTERVAL '1 day')
    ORDER BY COALESCE(removed_at, used_at), organization_id, value
    LIMIT GREATEST($3 - (SELECT count(*) FROM expired), 0)
    FOR UPDATE SKIP LOCKED
), removed_legacy AS (
    DELETE FROM cargo_suggestions AS suggestion
    USING legacy_expired
    WHERE suggestion.organization_id = legacy_expired.organization_id
      AND suggestion.value = legacy_expired.value
    RETURNING suggestion.value
), removed_customer AS (
    DELETE FROM customer_cargo_suggestions AS suggestion
    USING expired
    WHERE suggestion.id = expired.id
    RETURNING suggestion.value
)
SELECT value FROM removed_customer
UNION ALL
SELECT value FROM removed_legacy;
