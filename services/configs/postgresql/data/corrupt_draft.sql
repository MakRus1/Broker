-- Поврежденная запись проверяет, что ошибка хранения не превращается в HTTP 400.
INSERT INTO config_drafts(id, document) OVERRIDING SYSTEM VALUE VALUES (900000, '{}');
