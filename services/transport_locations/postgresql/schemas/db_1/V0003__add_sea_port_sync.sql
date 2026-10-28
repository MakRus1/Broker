-- Расписание и аренды общие для независимых источников справочника.
ALTER TABLE airport_sync RENAME TO directory_sync;
ALTER TABLE directory_sync ADD COLUMN source_revision TEXT;
INSERT INTO directory_sync (name, next_attempt_at) VALUES ('unlocode', now());

CREATE TABLE sea_port_sources (
    locode TEXT PRIMARY KEY,
    location_id UUID NOT NULL UNIQUE REFERENCES route_locations(id)
        DEFERRABLE INITIALLY DEFERRED,
    name TEXT NOT NULL,
    country_code TEXT NOT NULL,
    search_terms TEXT NOT NULL
);

CREATE TYPE public.sea_port_source_v1 AS (
    locode TEXT,
    location_id UUID,
    name TEXT,
    country_code TEXT,
    search_terms TEXT
);
