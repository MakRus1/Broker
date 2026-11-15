-- $1 — область каталога (global или идентификатор организации).
-- $2 — новый черновик каталога переводов в JSON.
UPDATE localization_editor SET version = version + 1, draft = $2::JSONB WHERE scope = $1;
