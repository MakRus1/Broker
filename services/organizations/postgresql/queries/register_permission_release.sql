INSERT INTO permission_releases (owner, version, entries, active)
VALUES ($1, $2, $3, TRUE)
ON CONFLICT (owner, version) DO UPDATE
SET active = TRUE;
