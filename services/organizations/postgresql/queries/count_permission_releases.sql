-- $1 — сервис-владелец каталога прав.
-- $2 — версии выпусков каталога прав.
SELECT COUNT(*)
FROM permission_releases
WHERE owner = $1 AND version = ANY($2::TEXT[]);
