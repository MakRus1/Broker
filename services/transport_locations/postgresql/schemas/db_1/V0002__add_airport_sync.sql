-- Ручная запись или правка остаётся приоритетнее внешнего справочника.
ALTER TABLE route_locations ADD COLUMN manual_override BOOLEAN NOT NULL DEFAULT FALSE;
UPDATE route_locations SET manual_override = TRUE;
ALTER TABLE route_locations ADD COLUMN search_terms TEXT;

CREATE TABLE airport_sources (
    source_id BIGINT PRIMARY KEY,
    location_id UUID NOT NULL UNIQUE REFERENCES route_locations(id)
        DEFERRABLE INITIALLY DEFERRED,
    name TEXT NOT NULL,
    country_code TEXT NOT NULL,
    iata_code TEXT,
    icao_code TEXT,
    search_terms TEXT NOT NULL
);

CREATE TYPE public.airport_source_v1 AS (
    source_id BIGINT,
    location_id UUID,
    name TEXT,
    country_code TEXT,
    iata_code TEXT,
    icao_code TEXT,
    search_terms TEXT
);

-- Короткая аренда не держит транзакцию во время HTTP-запроса.
CREATE TABLE airport_sync (
    name TEXT PRIMARY KEY,
    lease UUID,
    next_attempt_at TIMESTAMPTZ NOT NULL,
    updated_at TIMESTAMPTZ,
    source_hash TEXT,
    record_count INTEGER
);
INSERT INTO airport_sync (name, next_attempt_at) VALUES ('ourairports', now());

DROP INDEX route_locations_registry_search_idx;
CREATE INDEX route_locations_registry_search_idx
    ON route_locations USING GIN (
        (lower(name || ' ' || COALESCE(code, '') || ' ' || COALESCE(search_terms, ''))
            COLLATE "C") public.gin_trgm_ops
    ) WHERE NOT archived;
