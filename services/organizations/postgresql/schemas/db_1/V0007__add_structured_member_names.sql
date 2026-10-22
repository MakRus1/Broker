-- Старые display_name не разбиваем по пробелам: порядок частей ФИО неизвестен.
ALTER TABLE memberships
    ADD COLUMN last_name TEXT,
    ADD COLUMN first_name TEXT,
    ADD COLUMN middle_name TEXT;

CREATE TYPE public.member_v2 AS (
    subject TEXT,
    username TEXT,
    display_name TEXT,
    email TEXT,
    role TEXT,
    enabled BOOLEAN,
    revision BIGINT,
    last_name TEXT,
    first_name TEXT,
    middle_name TEXT
);
