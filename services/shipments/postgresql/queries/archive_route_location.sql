UPDATE route_locations SET archived = TRUE
WHERE id = $1::UUID AND NOT archived RETURNING id;
