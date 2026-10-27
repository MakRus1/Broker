-- Поиск по вхождению использует отдельный индекс, а не индекс порядка.
CREATE EXTENSION IF NOT EXISTS pg_trgm WITH SCHEMA public;
CREATE INDEX transportations_registry_search_idx ON transportations USING GIN
    ((lower(COALESCE(container_number, '')) COLLATE "C") public.gin_trgm_ops);
