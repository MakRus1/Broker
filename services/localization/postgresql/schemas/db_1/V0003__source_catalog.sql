-- Исходные тексты регистрируются пайплайном перед деплоем фронтенда.
-- Отсутствующие в новом релизе ключи сохраняются для старых страниц и отката.
CREATE TABLE localization_source (
    key TEXT PRIMARY KEY,
    ru TEXT NOT NULL,
    en TEXT NOT NULL,
    parameters TEXT[] NOT NULL,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
