-- $1 — область каталога (global или идентификатор организации).
SELECT scope, version, published_revision, draft FROM localization_editor
WHERE lower(scope) = $1
ORDER BY (scope = $1) DESC, scope
LIMIT 1;
