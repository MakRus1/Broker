CREATE INDEX railway_stations_registry_name_idx
    ON railway_stations ((lower(name) COLLATE "C"), esr_code)
    WHERE NOT archived;

CREATE INDEX railway_stations_registry_search_idx
    ON railway_stations USING GIN ((lower(name || ' ' || esr_code) COLLATE "C") public.gin_trgm_ops)
    WHERE NOT archived;
