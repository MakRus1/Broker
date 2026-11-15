-- $1 — область каталога (global или идентификатор организации).
INSERT INTO localization_editor (scope) VALUES ($1) ON CONFLICT DO NOTHING;
