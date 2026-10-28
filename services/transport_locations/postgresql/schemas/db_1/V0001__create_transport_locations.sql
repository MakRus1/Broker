CREATE TABLE route_locations (
    id UUID PRIMARY KEY,
    kind TEXT NOT NULL,
    name TEXT NOT NULL,
    country_code TEXT NOT NULL,
    code TEXT,
    archived BOOLEAN NOT NULL DEFAULT FALSE
);

CREATE UNIQUE INDEX route_locations_code
ON route_locations (kind, country_code, code) WHERE code IS NOT NULL;

CREATE TYPE public.transport_location_v1 AS (
    id UUID,
    kind TEXT,
    name TEXT,
    country_code TEXT,
    code TEXT
);


CREATE TABLE railway_stations (
    esr_code TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    country_code TEXT NOT NULL,
    archived BOOLEAN NOT NULL DEFAULT FALSE
);

CREATE TYPE public.railway_station_v1 AS (
    esr_code TEXT,
    name TEXT,
    country_code TEXT
);


CREATE EXTENSION IF NOT EXISTS pg_trgm WITH SCHEMA public;

CREATE INDEX route_locations_registry_name_idx
    ON route_locations (kind, (lower(name) COLLATE "C"), id)
    WHERE NOT archived;

CREATE INDEX route_locations_registry_search_idx
    ON route_locations USING GIN (
        (lower(name || ' ' || COALESCE(code, '')) COLLATE "C") public.gin_trgm_ops
    ) WHERE NOT archived;

CREATE INDEX railway_stations_registry_name_idx
    ON railway_stations ((lower(name) COLLATE "C"), esr_code)
    WHERE NOT archived;

CREATE INDEX railway_stations_registry_search_idx
    ON railway_stations USING GIN ((lower(name || ' ' || esr_code) COLLATE "C") public.gin_trgm_ops)
    WHERE NOT archived;

-- Однократный перенос из прежних владельцев, без повторного затирания правок.
CREATE TABLE directory_transfers (
    name TEXT PRIMARY KEY,
    completed_at TIMESTAMPTZ NOT NULL
);
