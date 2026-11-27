-- $1 — организация, $2 — идентификатор рейса.
SELECT (port).name, position FROM line_operations.voyage_port_calls
WHERE organization_id = $1 AND voyage_id = $2 ORDER BY position;
