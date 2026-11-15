-- $1 — сервис-владелец каталога прав.
-- $2 — версия выпуска каталога прав.
UPDATE permission_releases
SET active = TRUE
WHERE owner = $1 AND version = $2;
