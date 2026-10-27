CREATE EXTENSION IF NOT EXISTS pg_trgm WITH SCHEMA public;
CREATE INDEX counterparties_registry_search_idx ON counterparties USING GIN
    ((lower(name || ' ' || COALESCE(legal_name, '') || ' ' || COALESCE(tax_id, '')) COLLATE "C") public.gin_trgm_ops);
