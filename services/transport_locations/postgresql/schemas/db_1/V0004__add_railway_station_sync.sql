-- UUID сохраняет идентичность станции при изменении внешнего кода.
-- Та же формула используется для переноса сохранённых маршрутов.
ALTER TABLE railway_stations ADD COLUMN id UUID;
UPDATE railway_stations SET id = md5('broker:railway:esr:' || esr_code)::uuid;
ALTER TABLE railway_stations DROP CONSTRAINT railway_stations_pkey;
ALTER TABLE railway_stations ADD PRIMARY KEY (id);
ALTER TABLE railway_stations ALTER COLUMN esr_code DROP NOT NULL;
ALTER TABLE railway_stations ADD COLUMN uic_code TEXT;
ALTER TABLE railway_stations ADD COLUMN esr_source TEXT;
UPDATE railway_stations SET esr_source = 'manual';
ALTER TABLE railway_stations ADD COLUMN manual_override BOOLEAN NOT NULL DEFAULT TRUE;
ALTER TABLE railway_stations ADD COLUMN search_terms TEXT NOT NULL DEFAULT '';
CREATE INDEX railway_stations_codes_idx ON railway_stations (esr_code, country_code);
CREATE INDEX railway_stations_uic_idx ON railway_stations (uic_code, country_code);
DROP INDEX railway_stations_registry_name_idx;
DROP INDEX railway_stations_registry_search_idx;
CREATE INDEX railway_stations_registry_name_idx
    ON railway_stations ((lower(name) COLLATE "C"), id) WHERE NOT archived;
CREATE INDEX railway_stations_search_v2_idx ON railway_stations
    USING gin (lower(name || ' ' || coalesce(esr_code, '') || ' ' || coalesce(uic_code, '') || ' ' || search_terms) COLLATE "C" gin_trgm_ops)
    WHERE NOT archived;
CREATE TYPE public.railway_station_v2 AS (
    id UUID, name TEXT, country_code TEXT, esr_code TEXT, uic_code TEXT, esr_source TEXT
);
CREATE TABLE railway_station_sources (
    source_id TEXT PRIMARY KEY,
    station_id UUID NOT NULL UNIQUE REFERENCES railway_stations(id) DEFERRABLE INITIALLY DEFERRED,
    name TEXT NOT NULL,
    country_code TEXT NOT NULL,
    esr_code TEXT,
    uic_code TEXT,
    search_terms TEXT NOT NULL
);
CREATE TYPE public.railway_station_source_v1 AS (
    source_id TEXT, station_id UUID, name TEXT, country_code TEXT,
    esr_code TEXT, uic_code TEXT, search_terms TEXT
);
INSERT INTO directory_sync (name, next_attempt_at) VALUES ('osm-railway', now());
