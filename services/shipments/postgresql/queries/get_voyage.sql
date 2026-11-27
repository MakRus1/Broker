-- $1 — организация, $2 — идентификатор рейса.
SELECT number FROM line_operations.voyages WHERE organization_id = $1 AND id = $2;
