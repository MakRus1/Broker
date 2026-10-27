CREATE INDEX route_locations_registry_name_idx
    ON route_locations (kind, (lower(name) COLLATE "C"), id)
    WHERE NOT archived;

CREATE INDEX route_locations_registry_search_idx
    ON route_locations USING GIN (
        (lower(name || ' ' || COALESCE(code, '')) COLLATE "C") public.gin_trgm_ops
    ) WHERE NOT archived;
