-- $1 — область каталога (global или идентификатор организации).
SELECT version, published_revision, draft FROM localization_editor WHERE scope = $1 FOR UPDATE;
