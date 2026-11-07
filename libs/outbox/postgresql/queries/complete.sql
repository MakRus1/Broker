DELETE FROM outbox
WHERE id = $1
  AND lease_token = $2
  AND generation = $3::BIGINT;
