-- $1 — идентификатор сообщения.
-- $2 — токен аренды доставки.
-- $3 — задержка на одну попытку в секундах.
-- $4 — максимальная задержка повтора в секундах.
UPDATE outbox
SET available_at = now() + make_interval(
        secs => LEAST($4::BIGINT, attempts::BIGINT * $3::BIGINT)::DOUBLE PRECISION),
    lease_token = NULL
WHERE id = $1
  AND lease_token = $2;
