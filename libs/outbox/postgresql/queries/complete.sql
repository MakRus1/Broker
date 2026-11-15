-- $1 — идентификатор сообщения.
-- $2 — токен аренды доставки.
-- $3 — поколение сообщения, полученное при захвате.
DELETE FROM outbox
WHERE id = $1
  AND lease_token = $2
  AND generation = $3::BIGINT;
