-- $1 — сервис-владелец каталога прав.
-- $2 — версии выпусков каталога, которые должны остаться активными.
UPDATE permission_releases
SET active = (version = ANY($2::TEXT[]))
WHERE owner = $1;
