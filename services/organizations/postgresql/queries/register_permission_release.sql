-- $1 — сервис-владелец каталога прав.
-- $2 — версия выпуска каталога прав.
-- $3 — права из выпуска каталога в JSON.
INSERT INTO permission_releases (owner, version, entries, active)
VALUES ($1, $2, $3, TRUE)
ON CONFLICT (owner, version) DO UPDATE
SET active = TRUE;
