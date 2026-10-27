CREATE EXTENSION IF NOT EXISTS pg_trgm WITH SCHEMA public;

CREATE INDEX organizations_registry_name_idx
    ON organizations ((lower(name) COLLATE "C"), id);

CREATE INDEX organizations_registry_search_idx
    ON organizations USING GIN ((lower(name) COLLATE "C") public.gin_trgm_ops);
