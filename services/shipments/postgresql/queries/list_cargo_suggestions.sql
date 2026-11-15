-- $1 — организация.
-- $2 — идентификатор клиента.
-- $3 — поисковая подстрока в нижнем регистре.
SELECT value
FROM customer_cargo_suggestions
WHERE organization_id = $1::UUID AND customer_id = $2::UUID
    AND NOT removed AND strpos(search_value, $3) > 0
ORDER BY used_at DESC, value
LIMIT 20;
