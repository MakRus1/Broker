DELETE FROM outbox
WHERE id = $1::UUID
  AND lease_token = $2::UUID
  AND generation = $3::BIGINT;
