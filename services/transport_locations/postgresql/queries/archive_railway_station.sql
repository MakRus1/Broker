UPDATE railway_stations SET archived = TRUE, manual_override = TRUE
WHERE id = $1::uuid AND NOT archived
RETURNING id;
