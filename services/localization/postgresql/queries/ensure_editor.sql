-- $1 — область каталога (global или идентификатор организации).
INSERT INTO localization_editor (scope)
SELECT $1
WHERE NOT EXISTS (SELECT 1 FROM localization_editor WHERE lower(scope) = $1)
ON CONFLICT DO NOTHING;
