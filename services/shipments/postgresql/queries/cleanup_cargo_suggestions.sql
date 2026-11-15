-- $1 — срок хранения активных подсказок без использования, в днях.
-- $2 — срок хранения удалённых подсказок, в днях.
-- $3 — максимальное число подсказок в пакете.
WITH expired AS (
    SELECT id
    FROM customer_cargo_suggestions
    WHERE (NOT removed AND used_at < CURRENT_TIMESTAMP - $1::INTEGER * INTERVAL '1 day')
        OR (removed AND removed_at < CURRENT_TIMESTAMP - $2::INTEGER * INTERVAL '1 day')
    ORDER BY COALESCE(removed_at, used_at), id
    LIMIT $3
    FOR UPDATE SKIP LOCKED
)
DELETE FROM customer_cargo_suggestions AS suggestion
USING expired
WHERE suggestion.id = expired.id
RETURNING suggestion.value;
