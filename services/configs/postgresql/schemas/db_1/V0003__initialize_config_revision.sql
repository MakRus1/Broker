-- В пустом хранилище фиксируем исходную ревизию со временем инициализации.
-- Пустой объект использует встроенные значения; опубликованные настройки не меняем.
-- Явный номер 0 не сдвигает identity: следующая публикация получит 1.
INSERT INTO config_revisions (revision, configs) OVERRIDING SYSTEM VALUE
SELECT 0, '{}'::JSONB
WHERE NOT EXISTS (SELECT 1 FROM config_revisions);
