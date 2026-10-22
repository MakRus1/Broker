SELECT value FROM cargo_suggestions
WHERE organization_id = $1::UUID AND NOT removed AND strpos(search_value, $2) > 0
ORDER BY used_at DESC, value LIMIT 20;
