-- Раньше публикация сохраняла и неизменённые встроенные значения, но не их версии.
-- Сохраняем эти конфиги: версия указывает на их первую настоящую публикацию.
INSERT INTO config_versions (name, revision)
SELECT keys.name, min(history.revision)
FROM config_revisions AS history
CROSS JOIN LATERAL jsonb_object_keys(history.configs) AS keys(name)
WHERE history.revision > 0
GROUP BY keys.name
ON CONFLICT (name) DO NOTHING;

-- Публикация происходит после захвата общей блокировки, а не в момент BEGIN.
ALTER TABLE config_revisions ALTER COLUMN created_at SET DEFAULT clock_timestamp();
