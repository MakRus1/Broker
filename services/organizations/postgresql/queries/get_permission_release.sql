-- $1 — сервис-владелец каталога прав.
-- $2 — версия выпуска каталога прав.
-- $3 — права из выпуска каталога в JSON для сравнения с сохранёнными правами.
SELECT normalized.entries @> $3::JSONB AND $3::JSONB @> normalized.entries AS matches
FROM permission_releases AS release
CROSS JOIN LATERAL (
    SELECT COALESCE(jsonb_agg(entry - 'owner'), '[]'::JSONB) AS entries
    FROM jsonb_array_elements(release.entries) AS entry
) AS normalized
WHERE release.owner = $1 AND release.version = $2;
