CREATE EXTENSION IF NOT EXISTS pg_trgm WITH SCHEMA public;
-- Один многоколоночный GIN поддерживает независимый поиск номера и груза.
CREATE INDEX shipments_registry_search_idx ON shipments USING GIN
    ((lower(number) COLLATE "C") public.gin_trgm_ops,
     (lower(COALESCE(cargo_description, '')) COLLATE "C") public.gin_trgm_ops);
