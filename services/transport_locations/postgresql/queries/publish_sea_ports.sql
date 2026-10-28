INSERT INTO route_locations (id, kind, name, country_code, code, search_terms)
SELECT
    source.location_id,
    'sea_port',
    source.name,
    source.country_code,
    source.locode,
    source.search_terms
FROM sea_port_sources AS source
JOIN unnest($1::TEXT[]) AS imported(locode) USING (locode)
ON CONFLICT (id) DO UPDATE SET
    name = CASE WHEN route_locations.manual_override
        THEN route_locations.name ELSE EXCLUDED.name END,
    country_code = CASE WHEN route_locations.manual_override
        THEN route_locations.country_code ELSE EXCLUDED.country_code END,
    code = CASE WHEN route_locations.manual_override
        THEN route_locations.code ELSE EXCLUDED.code END,
    search_terms = EXCLUDED.search_terms;
