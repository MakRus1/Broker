-- $1 — организация.
SELECT id, number FROM line_operations.voyages WHERE organization_id = $1 ORDER BY number, id;
